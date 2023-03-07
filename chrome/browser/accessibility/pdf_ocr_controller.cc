// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/pdf_ocr_controller.h"

#include "base/check_op.h"
#include "chrome/browser/accessibility/ax_screen_ai_annotator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router_factory.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"

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

  // Annotator function of ScreenAI service requires AXScreenAIAnnotator to be
  // ready to receive OCR accessibility tree data.
  screen_ai::AXScreenAIAnnotatorFactory::EnsureExistsForBrowserContext(
      profile_);

  component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());

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
  DCHECK(web_contents);
  // `web_contents` should be a PDF Viewer Mimehandler.
  DCHECK_EQ(web_contents->GetContentsMimeType(), kHtmlMimeType);

  ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
  ax_mode.set_mode(ui::AXMode::kPDFOcr, true);
  web_contents->SetAccessibilityMode(ax_mode);
}

bool PdfOcrController::IsEnabled() const {
  return profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityPdfOcrAlwaysActive);
}

void PdfOcrController::OnPdfOcrAlwaysActiveChanged() {
  bool is_always_active =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityPdfOcrAlwaysActive);
  VLOG(2) << "PDF OCR Always Active changed: " << is_always_active;

  if (is_always_active) {
    // If Screen AI service is not ready and user is requesting OCR, keep the
    // request until service is up.
    if (screen_ai::ScreenAIInstallState::GetInstance()->get_state() !=
        ScreenAIInstallState::State::kReady) {
      // TODO(crbug.com/1393069): Consider letting user know that OCR will run
      // when service is ready.
      send_always_active_state_when_service_is_ready_ = true;
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

void PdfOcrController::StateChanged(ScreenAIInstallState::State state) {
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
      break;

    case ScreenAIInstallState::State::kDownloading:
      break;

    case ScreenAIInstallState::State::kFailed:
      // TODO(crbug.com/1393069): Disable menu items.
      break;

    case ScreenAIInstallState::State::kDownloaded:
      screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
          ->LaunchIfNotRunning();
      break;

    case ScreenAIInstallState::State::kReady:
      if (send_always_active_state_when_service_is_ready_) {
        send_always_active_state_when_service_is_ready_ = false;
        SendPdfOcrAlwaysActiveToAll(true);
      }
  }
}

}  // namespace screen_ai
