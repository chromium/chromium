// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller.h"

#include <vector>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "pdf/pdf_features.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif

namespace {

constexpr uint32_t kMaxInitializationRetry = 3;
constexpr base::TimeDelta kRetryDelay = base::Minutes(5);

// Returns all WebContents with PDF content associated with a given Profile.
// When a PDF is opened in GuestView PDF Viewer, the following structure is
// expected:
// -----------------------------------------
// WebContents A:
//  Primary main frame
//   WebContents B (inner PDF WebContents):
//    PDF extension frame
//     PDF content frame (renderer)
// -----------------------------------------
// On the other hand, OOPIF PDF Viewer doesn't create an inner WebContents.
// When a PDF is opened in OOPIF PDF Viewer, the following structure is
// expected:
// -----------------------------------------
// WebContents A:
//  Primary main frame
//   PDF extension frame
//    PDF content frame (renderer)
// -----------------------------------------
std::vector<content::WebContents*> GetAllPdfWebContents(Profile* profile) {
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

    if (chrome_pdf::features::IsOopifPdfEnabled()) {
      // If `web_contents` has a `pdf::PdfViewerStreamManager`, then there must
      // be a PDF in the WebContents in OOPIF PDF Viewer.
      if (pdf::PdfViewerStreamManager::FromWebContents(web_contents)) {
        result.push_back(web_contents);
      }
    } else if (IsPdfExtensionOrigin(web_contents->GetPrimaryMainFrame()
                                        ->GetLastCommittedOrigin())) {
      // GuestView PDF Viewer case. If the WebContents has a PDF, GuestView PDF
      // Viewer has one inner PDF WebContents, and its primary main frame has
      // the PDF extension origin. It will iterate on this innter PDF
      // WebContents, so check its primary main frame.
      result.push_back(web_contents);
    }
  }
  return result;
}

// Returns true if a screen reader is present, if the screen reader AXMode is
// enabled on any PDF web contents, or (on Chrome OS only) if select-to-speak is
// enabled.
bool IsAccessibilityEnabled(Profile* profile) {
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

  // Check all web contentses. `ReadAnythingUntrustedPageHandler` sets the
  // screen reader mode when starting to observe a PDF WebContents via
  // `SetUpPdfObserver()`. So if any of them have screen reader mode enabled,
  // return true.
  for (auto* contents : GetAllPdfWebContents(profile)) {
    if (contents->GetAccessibilityMode().has_mode(ui::AXMode::kScreenReader)) {
      return true;
    }
  }

  return false;
}

void RecordAcceptLanguages(const std::string& accept_languages) {
  for (std::string language :
       base::SplitString(accept_languages, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    // Convert to a Chrome language code synonym. This language synonym is then
    // converted into a `LocaleCodeISO639` enum value for a UMA histogram. See
    // tools/metrics/histograms/enums.xml enum LocaleCodeISO639. The enum there
    // doesn't always have locales where the base lang and the locale are the
    // same (e.g. they don't have id-id, but do have id). So if the base lang
    // and the locale are the same, just use the base lang.
    std::string language_to_log = language;
    std::vector<std::string> lang_split =
        base::SplitString(base::ToLowerASCII(language_to_log), "-",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (lang_split.size() == 2 && lang_split[0] == lang_split[1]) {
      language_to_log = lang_split[0];
    }
    language::ToChromeLanguageSynonym(&language_to_log);
    // TODO(crbug.com/40267312): Add a browser test to validate this UMA metric.
    base::UmaHistogramSparse("Accessibility.PdfOcr.UserAcceptLanguage",
                             base::HashMetricName(language_to_log));
  }
}

}  // namespace

namespace screen_ai {

PdfOcrController::PdfOcrController(Profile* profile)
    : profile_(profile), initialization_retry_wait_(kRetryDelay) {
  DCHECK(profile_);

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
  // TODO(crbug.com/289010799): Observe Chrome OS's select-to-speak setting.
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Trigger if a screen reader or Select-to-Speak on ChromeOS is enabled.
  OnActivationChanged();
}

PdfOcrController::~PdfOcrController() = default;

// static
std::vector<content::WebContents*>
PdfOcrController::GetAllPdfWebContentsForTesting(Profile* profile) {
  return GetAllPdfWebContents(profile);
}

bool PdfOcrController::IsEnabled() const {
  return scoped_accessibility_mode_ != nullptr;
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
  // PDF Searchify feature performs OCR on all inaccessible PDFs regardless of
  // accessibility settings. Therefore if it is enabled, we don't need to enable
  // OCR in PDF viewer.
  // TODO(crbug.com/360803943): Remove this class when PDF Searchify is
  // launched.
  bool enable =
      (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfSearchify) &&
       IsAccessibilityEnabled(profile_));

  if (enable == IsEnabled()) {
    return;  // No change in activation.
  }

  if (enable) {
    RecordAcceptLanguages(
        profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));

    if (!ocr_service_ready_) {
      InitializeService();
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

void PdfOcrController::InitializeService() {
  // Avoid repeated requests.
  if (waiting_for_ocr_service_initialization_) {
    return;
  }
  waiting_for_ocr_service_initialization_ = true;

  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
      ->GetServiceStateAsync(
          ScreenAIServiceRouter::Service::kOCR,
          base::BindOnce(&PdfOcrController::OCRServiceInitializationCallback,
                         weak_ptr_factory_.GetWeakPtr()));
}

void PdfOcrController::OCRServiceInitializationCallback(bool successful) {
  waiting_for_ocr_service_initialization_ = false;
  ocr_service_ready_ = successful;
  if (successful) {
    OnActivationChanged();
    base::UmaHistogramCounts100(
        "Accessibility.ScreenAI.Component.InstallRetries",
        initialization_retries_);
  } else {
    // Schedule a retry.
    initialization_retries_++;
    if (initialization_retries_ < kMaxInitializationRetry) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PdfOcrController::InitializeService,
                         weak_ptr_factory_.GetWeakPtr()),
          initialization_retry_wait_);
    }
  }
}

void PdfOcrController::Activate() {
  OnActivationChanged();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PdfOcrController::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kScreenReader)) {
    OnActivationChanged();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace screen_ai
