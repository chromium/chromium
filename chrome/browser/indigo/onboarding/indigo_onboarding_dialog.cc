// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
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
class OnboardingDialogTracker
    : public content::WebContentsUserData<OnboardingDialogTracker> {
 public:
  ~OnboardingDialogTracker() override = default;
  IndigoOnboardingDialog* dialog() { return dialog_.get(); }

 private:
  friend class content::WebContentsUserData<OnboardingDialogTracker>;

  explicit OnboardingDialogTracker(content::WebContents* web_contents,
                                   base::WeakPtr<IndigoOnboardingDialog> dialog)
      : content::WebContentsUserData<OnboardingDialogTracker>(*web_contents),
        dialog_(std::move(dialog)) {}

  base::WeakPtr<IndigoOnboardingDialog> dialog_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(OnboardingDialogTracker);

constexpr gfx::Size kMinSize{480, 360};
constexpr gfx::Size kMaxSize{480, 600};

class OnboardingWebView : public views::WebView {
 public:
  using WebView::WebView;

  void SetOnCloseCallback(base::OnceClosure close_callback) {
    close_callback_ = std::move(close_callback);
  }

  void SetBrowserWindowCallback(
      base::RepeatingCallback<BrowserWindowInterface*()>
          browser_window_callback) {
    browser_window_callback_ = std::move(browser_window_callback);
  }

  // WebContentsDelegate:

  void CloseContents(content::WebContents* web_contents) override {
    if (close_callback_) {
      std::move(close_callback_).Run();
    }
  }

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

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    if (BrowserWindowInterface* browser = GetBrowserWindow()) {
      return browser->OpenURL(params, std::move(navigation_handle_callback));
    }
    return nullptr;
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    if (BrowserWindowInterface* browser = GetBrowserWindow()) {
      return chrome::AddWebContents(browser, source, std::move(new_contents),
                                    target_url, disposition, window_features);
    }
    return nullptr;
  }

 private:
  BrowserWindowInterface* GetBrowserWindow() {
    return browser_window_callback_ ? browser_window_callback_.Run() : nullptr;
  }

  base::RepeatingCallback<BrowserWindowInterface*()> browser_window_callback_;
  base::OnceClosure close_callback_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoOnboardingDialog, kWebViewId);

// static
std::unique_ptr<IndigoOnboardingDialog> IndigoOnboardingDialog::Show(
    tabs::TabInterface& tab,
    const GURL& onboarding_url,
    base::OnceCallback<void(const OnboardingResult&)> close_callback) {
  if (!tab.CanShowModalUI()) {
    return nullptr;
  }
  return base::WrapUnique(new IndigoOnboardingDialog(
      tab, onboarding_url, std::move(close_callback)));
}

IndigoOnboardingDialog::IndigoOnboardingDialog(
    tabs::TabInterface& tab,
    const GURL& onboarding_url,
    base::OnceCallback<void(const OnboardingResult&)> close_callback)
    : tab_(&tab), close_callback_(std::move(close_callback)) {
  Profile* profile =
      Profile::FromBrowserContext(tab.GetContents()->GetBrowserContext());
  auto web_view = std::make_unique<OnboardingWebView>(profile);
  web_view->SetOnCloseCallback(
      base::BindOnce(&IndigoOnboardingDialog::Close, base::Unretained(this)));
  web_view->SetBrowserWindowCallback(base::BindRepeating(
      [](const base::WeakPtr<tabs::TabInterface>& tab) {
        return tab ? tab->GetBrowserWindowInterface() : nullptr;
      },
      tab.GetWeakPtr()));

  OnboardingDialogTracker::CreateForWebContents(web_view->GetWebContents(),
                                                weak_ptr_factory_.GetWeakPtr());

  // Force preferences update to pick up the tracker and set
  // is_indigo_onboarding.
  web_view->GetWebContents()->NotifyPreferencesChanged();

  content::NavigationController::LoadURLParams load_params(onboarding_url);
  load_params.extra_headers = "X-Chrome-Onboarding: ?1";
  web_view->GetWebContents()->GetController().LoadURLWithParams(load_params);
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
  // Stop observing the view before it is destroyed by the widget.
  view_observation_.Reset();

  // As recommended in the comment on `views::Widget::MakeCloseSynchronous`,
  // destroy the widget here.
  widget_.reset();

  if (close_callback_) {
    std::move(close_callback_).Run(onboarding_result_);
  }
}

// static
void IndigoOnboardingDialog::BindOnboardingDialogHost(
    mojo::PendingAssociatedReceiver<chrome::mojom::IndigoOnboardingDialogHost>
        receiver,
    content::RenderFrameHost* render_frame_host) {
  // Note: This method can be called while `render_frame_host` is still
  // speculative. There is no good way to check if it's speculative outside of
  // content/, so we just fallback to using GetParentOrOuterDocument instead
  // of !IsInPrimaryMainFrame(). We do ensure the RFH is primary (and no longer
  // speculative) when processing subsequent IPC calls (i.e. in
  // AcknowledgeChromeDisclaimer()).
  if (render_frame_host->GetParentOrOuterDocument()) {
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }
  OnboardingDialogTracker* tracker =
      OnboardingDialogTracker::FromWebContents(web_contents);
  if (!tracker) {
    return;
  }
  if (IndigoOnboardingDialog* dialog = tracker->dialog()) {
    dialog->receiver_set_.Add(dialog, std::move(receiver),
                              render_frame_host->GetGlobalId());
  }
}

void IndigoOnboardingDialog::AcknowledgeChromeDisclaimer() {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(receiver_set_.current_context());
  // A primary main frame can become non-primary by being stored in bfcache.
  // It shouldn't be able to acknowledge while in such a state.
  if (!rfh || !rfh->IsInPrimaryMainFrame()) {
    return;
  }
  CHECK_EQ(content::WebContents::FromRenderFrameHost(rfh),
           static_cast<views::WebView*>(delegate_->GetContentsView())
               ->GetWebContents());
  onboarding_result_.acknowledge_chrome_disclaimer = true;
}

// static
bool IndigoOnboardingDialog::IsOnboardingWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kIndigo)) {
    return false;
  }
  return OnboardingDialogTracker::FromWebContents(web_contents) != nullptr;
}

}  // namespace indigo
