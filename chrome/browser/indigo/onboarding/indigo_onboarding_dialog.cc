// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace indigo {

namespace {
constexpr gfx::Size kMinSize{480, 360};
constexpr gfx::Size kMaxSize{480, 600};

class OnboardingWebView : public views::WebView {
 public:
  using WebView::WebView;

  // WebContentsDelegate:

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                     params);
  }

  bool HandleKeyboardEvent(
      content::WebContents* web_contents,
      const input::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

 private:
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoOnboardingDialog, kWebViewId);

// static
std::unique_ptr<IndigoOnboardingDialog> IndigoOnboardingDialog::Show(
    tabs::TabInterface& tab,
    const GURL& onboarding_url,
    base::OnceClosure close_callback) {
  if (!tab.CanShowModalUI()) {
    return nullptr;
  }
  return base::WrapUnique(new IndigoOnboardingDialog(
      tab, onboarding_url, std::move(close_callback)));
}

IndigoOnboardingDialog::IndigoOnboardingDialog(tabs::TabInterface& tab,
                                               const GURL& onboarding_url,
                                               base::OnceClosure close_callback)
    : tab_(&tab), close_callback_(std::move(close_callback)) {
  Profile* profile =
      Profile::FromBrowserContext(tab.GetContents()->GetBrowserContext());
  auto web_view = std::make_unique<OnboardingWebView>(profile);
  web_view->GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(onboarding_url));
  web_view->SetPreferredSize(kMinSize);
  web_view->EnableSizingFromWebContents(kMinSize, kMaxSize);
  web_view->SetProperty(views::kElementIdentifierKey, kWebViewId);

  view_observation_.Observe(web_view.get());

  delegate_ = std::make_unique<views::DialogDelegate>();
  delegate_->SetContentsView(std::move(web_view));
  delegate_->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate_->SetShowCloseButton(true);
  delegate_->SetModalType(ui::mojom::ModalType::kChild);
  delegate_->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto* tab_dialog_manager = tab.GetTabFeatures()->tab_dialog_manager();
  widget_ = tab_dialog_manager->CreateTabScopedDialog(delegate_.get());
  widget_->MakeCloseSynchronous(base::BindOnce(
      &IndigoOnboardingDialog::OnWidgetClosed, base::Unretained(this)));

  auto params = std::make_unique<tabs::TabDialogManager::Params>();
  tab_dialog_manager->ShowDialog(widget_.get(), std::move(params));
}

IndigoOnboardingDialog::~IndigoOnboardingDialog() = default;

void IndigoOnboardingDialog::Close() {
  if (widget_) {
    widget_->Close();
  }
}

void IndigoOnboardingDialog::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  tab_->GetTabFeatures()->tab_dialog_manager()->UpdateModalDialogBounds();
}

void IndigoOnboardingDialog::OnWidgetClosed(
    views::Widget::ClosedReason reason) {
  // As recommended in the comment on `views::Widget::MakeCloseSynchronous`,
  // destroy the widget here.
  widget_.reset();

  if (close_callback_) {
    std::move(close_callback_).Run();
  }
}

}  // namespace indigo
