// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

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

void PdfOcrController::RunPdfOcrOnlyOnce(content::WebContents* web_contents) {
  // TODO(crbug.com/1393069): Need to wait for the Screen AI library to be
  // installed if not ready yet. Then, set the AXMode for PDF OCR only when the
  // Screen AI library is downloaded and ready.
  if (!web_contents) {
    CHECK_IS_TEST();
    return;
  }

  if (MaybeAddObserverOrTriggerDownload(web_contents)) {
    // If we added an observer for `ScreenAIInstallState` or triggered
    // downloading the Screen AI library, return here; the request will be
    // handled when the library is ready or discarded if it fails to load it.
    return;
  }

  // `web_contents` should be a PDF Viewer Mimehandler.
  DCHECK_EQ(web_contents->GetContentsMimeType(), kHtmlMimeType);

  ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
  ax_mode.set_mode(ui::AXMode::kPDFOcr, true);
  web_contents->SetAccessibilityMode(ax_mode);
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

  if (is_always_active) {
    if (MaybeAddObserverOrTriggerDownload(
            /*web_contents_for_only_once_request=*/nullptr)) {
      // If we added an observer for `ScreenAIInstallState` or triggered
      // downloading the Screen AI library, return here; the request will be
      // handled when the library is ready or discarded if it fails to load it.
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

bool PdfOcrController::MaybeAddObserverOrTriggerDownload(
    content::WebContents* web_contents_for_only_once_request) {
  ScreenAIInstallState::State current_install_state =
      ScreenAIInstallState::GetInstance()->get_state();

  if (current_install_state == ScreenAIInstallState::State::kReady) {
    return false;
  }

  if (!component_ready_observer_.IsObserving()) {
    // Start observing ScreenAIInstallState when the user activates PDF OCR. It
    // triggers downloading the Screen AI library if it's not downloaded. Keep
    // the request until the library is ready.
    if (web_contents_for_only_once_request) {
      // PDF OCR once request. Keep its weak pointer of the web contents
      // requested for this.
      last_webcontents_requested_for_run_once_ =
          web_contents_for_only_once_request->GetWeakPtr();
    } else {
      // PDF OCR always request.
      send_always_active_state_when_service_is_ready_ = true;
    }
    component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
    return true;
  }

  if (current_install_state == ScreenAIInstallState::State::kFailed) {
    // Try downloading the Screen AI library again if it failed before. Keep the
    // request until the library is ready; this request will be discarded
    // if it fails to download it again.
    // TODO(crbug.com/127829): Make sure requesting a failed download will
    // trigger a new one.
    if (web_contents_for_only_once_request) {
      // PDF OCR once request. Keep its weak pointer of the web contents
      // requested for this.
      last_webcontents_requested_for_run_once_ =
          web_contents_for_only_once_request->GetWeakPtr();
    } else {
      // PDF OCR always request.
      send_always_active_state_when_service_is_ready_ = true;
    }
    ScreenAIInstallState::GetInstance()->DownloadComponent();
    return true;
  }

  return false;
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
      if (last_webcontents_requested_for_run_once_) {
        last_webcontents_requested_for_run_once_.reset();
      }
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
      if (last_webcontents_requested_for_run_once_) {
        RunPdfOcrOnlyOnce(last_webcontents_requested_for_run_once_.get());
        last_webcontents_requested_for_run_once_.reset();
      }
      if (send_always_active_state_when_service_is_ready_) {
        send_always_active_state_when_service_is_ready_ = false;
        SendPdfOcrAlwaysActiveToAll(true);
      }
  }
}

}  // namespace screen_ai
