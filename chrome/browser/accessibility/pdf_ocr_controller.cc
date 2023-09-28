// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

constexpr char kHtmlMimeType[] = "text/html";

// For a PDF tab, there are two associated processes (and two WebContentses):
// (i) PDF Viewer Mimehandler (mime type = text/html) and (ii) PDF renderer
// process (mime type = application/pdf). This helper function returns all PDF-
// related WebContentses associated with the Mimehandlers for a given Profile.
// Note that it does trigger PdfAccessibilityTree::AccessibilityModeChanged()
// if the AXMode with ui::AXMode::kPDFOcr is set on PDF WebContents with the
// text/html mime type; but it does not on PDF WebContents with the
// application/pdf mime type.
std::vector<content::WebContents*> GetPdfHtmlWebContentses(Profile* profile) {
  // Code borrowed from `content::WebContentsImpl::GetAllWebContents()`.
  std::vector<content::WebContents*> result;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  // Iterate over all RWHs and their RVHs and store a WebContents if the
  // WebContents is associated with PDF Viewer Mimehandler and belongs to the
  // given Profile.
  while (content::RenderWidgetHost* rwh = widgets->GetNextHost()) {
    content::RenderViewHost* rvh = content::RenderViewHost::From(rwh);
    if (!rvh) {
      continue;
    }
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (!web_contents) {
      continue;
    }
    if (profile !=
        Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
      continue;
    }
    // Check if WebContents is PDF's.
    if (!IsPdfExtensionOrigin(
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
      continue;
    }
    DCHECK_EQ(web_contents->GetContentsMimeType(), kHtmlMimeType);
    result.push_back(web_contents);
  }
  return result;
}

// Invoke screen reader alert to notify the user of the state.
void AnnounceToScreenReader(const int message_id) {
  const Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser) {
    VLOG(2) << "Browser is not ready to announce";
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    VLOG(2) << "Browser is not ready to announce";
    return;
  }

  browser_view->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(message_id));
}

void RecordAcceptLanguages(const std::string& accept_languages) {
  for (std::string language :
       base::SplitString(accept_languages, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    // Convert to a Chrome language code synonym. This language synonym is then
    // converted into a `LocaleCodeISO639` enum value for a UMA histogram.
    language::ToChromeLanguageSynonym(&language);
    // TODO(crbug.com/1443345): Add a browser test to validate this UMA metric.
    base::UmaHistogramSparse("Accessibility.PdfOcr.UserAcceptLanguage",
                             base::HashMetricName(language));
  }
}

}  // namespace

namespace screen_ai {

PdfOcrController::PdfOcrController(Profile* profile) : profile_(profile) {
  // Initialize an observer for changes of PDF OCR pref.
  DCHECK(profile_);
  VLOG(2) << "Init PdfOcrController";
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityPdfOcrAlwaysActive,
      base::BindRepeating(&PdfOcrController::OnPdfOcrAlwaysActiveChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Trigger if the preference is already set.
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kAccessibilityPdfOcrAlwaysActive)) {
    OnPdfOcrAlwaysActiveChanged();
  }
}

PdfOcrController::~PdfOcrController() = default;

// static
std::vector<content::WebContents*>
PdfOcrController::GetAllPdfWebContentsesForTesting(Profile* profile) {
  return GetPdfHtmlWebContentses(profile);
}

bool PdfOcrController::IsEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
             prefs::kAccessibilityPdfOcrAlwaysActive) &&
         !send_always_active_state_when_service_is_ready_;
}

void PdfOcrController::OnPdfOcrAlwaysActiveChanged() {
  bool is_always_active =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive);
  VLOG(2) << "PDF OCR Always Active changed: " << is_always_active;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This preference should be kept in sync with Ash.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    VLOG(0) << "Cannot sync the preference with Ash.";
  } else {
    lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
        crosapi::mojom::PrefPath::kAccessibilityPdfOcrAlwaysActive,
        profile_->GetPrefs()
            ->GetValue(prefs::kAccessibilityPdfOcrAlwaysActive)
            .Clone(),
        base::OnceClosure());
  }
#endif

  if (is_always_active) {
    RecordAcceptLanguages(
        profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
    if (MaybeScheduleRequest()) {
      // The request will be handled when the library is ready or discarded if
      // it fails to load.
      return;
    }
  } else {
    // If user has previously requested Always Active and the service was not
    // ready then, and now user has untoggeled it, ignore both requests.
    if (send_always_active_state_when_service_is_ready_) {
      send_always_active_state_when_service_is_ready_ = false;
      return;
    }
  }

  SendPdfOcrAlwaysActiveToAll(is_always_active);
}

void PdfOcrController::SendPdfOcrAlwaysActiveToAll(bool is_always_active) {
  std::vector<content::WebContents*> html_web_contents_vector =
      GetPdfHtmlWebContentses(profile_);
  // Iterate over all WebContentses associated with PDF Viewer Mimehandlers and
  // set the AXMode with the ui::AXMode::kPDFOcr flag.
  for (auto* web_contents : html_web_contents_vector) {
    ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
    ax_mode.set_mode(ui::AXMode::kPDFOcr, is_always_active);
    web_contents->SetAccessibilityMode(ax_mode);
  }
}

bool PdfOcrController::MaybeScheduleRequest() {
  ScreenAIInstallState::State current_install_state =
      ScreenAIInstallState::GetInstance()->get_state();

  // No need for scheduling if service is ready already.
  if (current_install_state == ScreenAIInstallState::State::kReady) {
    return false;
  }

  // Keep the request until the library is ready.
  send_always_active_state_when_service_is_ready_ = true;

  // TODO(crbug.com/127829): Make sure requesting to repeat a failed download
  // will trigger a new one.
  if (current_install_state == ScreenAIInstallState::State::kFailed) {
    ScreenAIInstallState::GetInstance()->DownloadComponent();
  }

  if (!component_ready_observer_.IsObserving()) {
    // Start observing ScreenAIInstallState when the user activates PDF OCR. It
    // triggers downloading the Screen AI library if it's not downloaded.
    component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
  }

  return true;
}

void PdfOcrController::StateChanged(ScreenAIInstallState::State state) {
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
      break;

    case ScreenAIInstallState::State::kDownloading:
      AnnounceToScreenReader(IDS_SETTINGS_PDF_OCR_DOWNLOADING);
      break;

    case ScreenAIInstallState::State::kFailed:
      AnnounceToScreenReader(IDS_SETTINGS_PDF_OCR_DOWNLOAD_ERROR);
      if (send_always_active_state_when_service_is_ready_) {
        // Update the PDF OCR pref to be false to toggle off the button.
        profile_->GetPrefs()->SetBoolean(
            prefs::kAccessibilityPdfOcrAlwaysActive, false);
        send_always_active_state_when_service_is_ready_ = false;
      }
      break;

    case ScreenAIInstallState::State::kDownloaded:
      AnnounceToScreenReader(IDS_SETTINGS_PDF_OCR_DOWNLOAD_COMPLETE);
      screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
          ->InitializeOCRIfNeeded();
      break;

    case ScreenAIInstallState::State::kReady:
      if (send_always_active_state_when_service_is_ready_) {
        send_always_active_state_when_service_is_ready_ = false;
        SendPdfOcrAlwaysActiveToAll(true);
      }
  }
}

}  // namespace screen_ai
