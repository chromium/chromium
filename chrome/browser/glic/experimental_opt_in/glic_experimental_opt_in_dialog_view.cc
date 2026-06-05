// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

namespace {

class GlicWebView : public views::WebView {
 public:
  explicit GlicWebView(Profile* profile, tabs::TabInterface* tab_interface)
      : views::WebView(profile), tab_interface_(tab_interface) {}
  ~GlicWebView() override = default;

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    // When a dialog is initialized with Auto-Resize enabled
    // (EnableSizingFromWebContents), Blink performs an initial layout pass on
    // the document. Because the web contents are still being loaded, parsed,
    // and rendering is not yet complete, the document height will not be the
    // desired one at this time and we should suppress until the webview is
    // ready.
    if (!received_first_resize_) {
      received_first_resize_ = true;
      return;
    }

    views::WebView::ResizeDueToAutoResize(source, new_size);
    if (GetWidget()) {
      tab_interface_->GetTabFeatures()
          ->tab_dialog_manager()
          ->UpdateModalDialogBounds();
    }
  }

 private:
  raw_ptr<tabs::TabInterface> tab_interface_;
  bool received_first_resize_ = false;
};

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicExperimentalOptInDialogView,
                                      kDialogElementId);

GlicExperimentalOptInDialogView::GlicExperimentalOptInDialogView(
    Profile* profile,
    tabs::TabInterface* tab_interface) {
  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kChild);
  set_esc_should_cancel_dialog_override(true);

  auto web_view = std::make_unique<GlicWebView>(profile, tab_interface);
  web_view_ = web_view.get();
  web_view->SetProperty(views::kElementIdentifierKey, kDialogElementId);

  RequiredExperimentalOptIn required_state =
      RequiredExperimentalOptIn::kExperimental;
  if (auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
    required_state = service->enabling().GetRequiredExperimentalOptIn();
  }
  if (required_state == RequiredExperimentalOptIn::kNotNeeded) {
    required_state = RequiredExperimentalOptIn::kExperimental;
  }

  gfx::Size initial_size(
      kGlicExperimentalOptInDefaultWidth,
      (required_state == RequiredExperimentalOptIn::kExperimental)
          ? kGlicExperimentalOptInDefaultHeightExperimental
          : kGlicExperimentalOptInDefaultHeightGlic);
  web_view->SetPreferredSize(initial_size);

  // Create WebContents for the webview.
  auto web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));

  // Load the experimental opt-in WebUI.
  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL(chrome::kChromeUIGlicExperimentalOptInURL)));

  web_view->SetOwnedWebContents(std::move(web_contents));

  // Enable auto-resizing from content, with min size as initial size and a max
  // size.
  web_view->EnableSizingFromWebContents(
      gfx::Size(kGlicExperimentalOptInDefaultWidth, 200),
      gfx::Size(kGlicExperimentalOptInDefaultWidth, 800));

  view_observation_.Observe(web_view_);
  SetContentsView(std::move(web_view));
}

GlicExperimentalOptInDialogView::~GlicExperimentalOptInDialogView() = default;

views::WebView* GlicExperimentalOptInDialogView::GetWebViewForTesting() {
  return web_view_;
}

void GlicExperimentalOptInDialogView::OnViewAddedToWidget(
    views::View* observed_view) {
  if (observed_view != web_view_) {
    return;
  }

  // Apply rounded corners to the NativeViewHost to prevent WebUI content
  // from bleeding through the dialog's rounded corners.
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void GlicExperimentalOptInDialogView::OnViewIsDeleting(
    views::View* observed_view) {
  if (observed_view == web_view_) {
    view_observation_.Reset();
    web_view_ = nullptr;
  }
}

}  // namespace glic
