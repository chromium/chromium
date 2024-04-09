// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/pdf/common/pdf_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif

namespace {

constexpr char kHtmlMimeType[] = "text/html";

// Returns true if a screen reader is present or (on Chrome OS only) if
// select-to-speak is enabled.
bool IsAccessibilityEnabled() {
  // Active if a screen reader is present.
  if (accessibility_state_utils::IsScreenReaderEnabled()) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Conditionally active if select-to-speak is enabled.
  if (features::IsAccessibilityPdfOcrForSelectToSpeakEnabled() &&
      accessibility_state_utils::IsSelectToSpeakEnabled()) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return false;
}

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
// TODO(crbug.com/333398721): Sending announcements results in a failure in
// `AuraLinuxAccessibilityInProcessBrowserTest::IndexInParentWithModal` and
// flaky fail when running Chrome.
#if !BUILDFLAG(IS_LINUX)
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
#endif
}

void RecordAcceptLanguages(const std::string& accept_languages) {
  for (std::string language :
       base::SplitString(accept_languages, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    // Convert to a Chrome language code synonym. This language synonym is then
    // converted into a `LocaleCodeISO639` enum value for a UMA histogram.
    language::ToChromeLanguageSynonym(&language);
    // TODO(crbug.com/1443346): Add a browser test to validate this UMA metric.
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

  // Register for changes to screenreader/spoken feedback/select to speak.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto* const accessibility_manager = ash::AccessibilityManager::Get();
      accessibility_manager) {
    // Unretained is safe because `this` owns the subscription.
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(
            base::BindRepeating(&PdfOcrController::OnAccessibilityStatusEvent,
                                base::Unretained(this)));
  }
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO: Observe Chrome OS's select-to-speak setting.
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Trigger if the preference is already set, and a screen reader or Select-to-
  // Speak on ChromeOS is enabled.
  OnActivationChanged();
}

PdfOcrController::~PdfOcrController() = default;

// static
std::vector<content::WebContents*>
PdfOcrController::GetAllPdfWebContentsesForTesting(Profile* profile) {
  return GetPdfHtmlWebContentses(profile);
}

bool PdfOcrController::IsEnabled() const {
  return scoped_accessibility_mode_ != nullptr;
}

void PdfOcrController::OnPdfOcrAlwaysActiveChanged() {
  const auto& pref_value =
      profile_->GetPrefs()->GetValue(prefs::kAccessibilityPdfOcrAlwaysActive);
  VLOG(2) << "PDF OCR Always Active changed: " << pref_value.GetBool();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This preference should be kept in sync with Ash.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    VLOG(0) << "Cannot sync the preference with Ash.";
  } else {
    lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
        crosapi::mojom::PrefPath::kAccessibilityPdfOcrAlwaysActive,
        pref_value.Clone(), base::OnceClosure());
  }
#endif

  OnActivationChanged();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PdfOcrController::OnAccessibilityStatusEvent(
    const ash::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
          ash::AccessibilityNotificationType::kToggleSpokenFeedback ||
      details.notification_type ==
          ash::AccessibilityNotificationType::kToggleSelectToSpeak) {
    OnActivationChanged();
  }
}
#endif  // BUIDLFLAG(IS_CHROMEOS_ASH)

void PdfOcrController::OnActivationChanged() {
  const bool is_always_active =
      IsAccessibilityEnabled() &&
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive);

  if (is_always_active == IsEnabled()) {
    return;  // No change in activation.
  }

  if (is_always_active) {
    RecordAcceptLanguages(
        profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));

    if (!ocr_service_ready_) {
      // Avoid repeated requests.
      if (waiting_for_ocr_service_initialization_) {
        return;
      }
      waiting_for_ocr_service_initialization_ = true;

      if (ScreenAIInstallState::GetInstance()->get_state() !=
              ScreenAIInstallState::State::kDownloaded &&
          !component_ready_observer_.IsObserving()) {
        // Start observing ScreenAIInstallState to report it to user.
        component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
      }

      screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
          ->GetServiceStateAsync(
              ScreenAIServiceRouter::Service::kOCR,
              base::BindOnce(
                  &PdfOcrController::OCRServiceInitializationCallback,
                  weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    // This will send the `kPDFOcr` flag to all WebContents. Strictly speaking,
    // it need only be sent to those associated with PDF Viewer Mimehandlers,
    // but we have no filtering mechanism today. The others should simply ignore
    // it.
    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForBrowserContext(profile_, ui::AXMode::kPDFOcr);
  } else {
    scoped_accessibility_mode_.reset();
  }
}

void PdfOcrController::OCRServiceInitializationCallback(bool successful) {
  waiting_for_ocr_service_initialization_ = false;
  ocr_service_ready_ = successful;
  if (successful) {
    OnActivationChanged();
  } else {
    // Call `StateChanged` to announce the state to user.
    StateChanged(ScreenAIInstallState::State::kDownloadFailed);
  }

  // No more need for observing Screen AI state changes.
  component_ready_observer_.Reset();
}

void PdfOcrController::StateChanged(ScreenAIInstallState::State state) {
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
      break;

    case ScreenAIInstallState::State::kDownloading:
      AnnounceToScreenReader(IDS_SETTINGS_PDF_OCR_DOWNLOADING);
      break;

    case ScreenAIInstallState::State::kDownloadFailed:
      AnnounceToScreenReader(IDS_SETTINGS_PDF_OCR_DOWNLOAD_ERROR);
      // Update the PDF OCR pref to be false to toggle off the button.
      profile_->GetPrefs()->SetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive,
                                       false);
      break;

    case ScreenAIInstallState::State::kDownloaded:
      AnnounceToScreenReader(IDS_SETTINGS_PDF_OCR_DOWNLOAD_COMPLETE);
      break;
  }
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PdfOcrController::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kScreenReader)) {
    OnActivationChanged();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace screen_ai
