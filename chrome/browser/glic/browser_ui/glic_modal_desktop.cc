// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/glic/browser_ui/glic_modal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/web_contents_delegate_views.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace glic {

namespace {

class GlicModalDialogView;

class GlicModalWebView : public views::WebView {
 public:
  GlicModalWebView(Profile* profile,
                   const tabs::TabInterface& tab,
                   bool suppress_first_auto_resize,
                   GlicModalDialogView* dialog_view)
      : views::WebView(profile),
        web_dialog_delegate_(profile,
                             std::make_unique<ChromeWebContentsHandler>()),
        tab_handle_(tab.GetHandle()),
        suppress_first_auto_resize_(suppress_first_auto_resize),
        dialog_view_(dialog_view) {}
  ~GlicModalWebView() override = default;

  void clear_dialog_view() { dialog_view_ = nullptr; }

  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    return web_dialog_delegate_.OpenURLFromTab(
        source, params, std::move(navigation_handle_callback));
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    return web_dialog_delegate_.AddNewContents(
        source, std::move(new_contents), target_url, disposition,
        window_features, user_gesture, was_blocked);
  }

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    web_dialog_delegate_.RunFileChooser(render_frame_host, std::move(listener),
                                        params);
  }

 private:
  ui::WebDialogWebContentsDelegate web_dialog_delegate_;
  const tabs::TabHandle tab_handle_;
  bool suppress_first_auto_resize_ = false;
  bool received_first_resize_ = false;
  raw_ptr<GlicModalDialogView> dialog_view_ = nullptr;
};

class GlicModalDialogView : public views::DialogDelegate,
                            public views::ViewObserver,
                            public views::WidgetObserver,
                            public content::WebContentsObserver {
 public:
  GlicModalDialogView(Profile* profile,
                      const tabs::TabInterface& tab,
                      GlicModal::Params& params)
      : tab_handle_(tab.GetHandle()),
        focus_web_view_on_open_(params.focus_web_view_on_open),
        accessible_title_(params.accessible_title) {
    SetShowCloseButton(false);
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    SetModalType(ui::mojom::ModalType::kChild);
    set_esc_should_cancel_dialog_override(true);
    SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    auto web_view = std::make_unique<GlicModalWebView>(
        profile, tab, params.suppress_first_auto_resize, this);
    web_view_ = web_view.get();
    web_view_->SetProperty(views::kElementIdentifierKey,
                           params.element_identifier
                               ? params.element_identifier
                               : GlicModal::kDialogElementId);
    web_view_->SetPreferredSize(params.initial_size);
    web_view_->SetAcceleratorHandling(
        views::WebContentsAcceleratorHandling::kEnabled);

    if (!accessible_title_.empty()) {
      SetAccessibleTitle(accessible_title_);
    }

    auto web_contents = content::WebContents::Create(
        content::WebContents::CreateParams(profile));
    web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

    if (focus_web_view_on_open_) {
      Observe(web_contents.get());
    }

    web_view_->SetOwnedWebContents(std::move(web_contents));
    if (params.size_to_content ||
        (!params.min_size.IsEmpty() && !params.max_size.IsEmpty() &&
         params.min_size != params.max_size)) {
      web_view_->EnableSizingFromWebContents(params.min_size, params.max_size);
    }
    web_view_->GetWebContents()->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params.content_url));

    view_observation_.Observe(web_view_);

    if (params.loading_view) {
      auto container = std::make_unique<views::View>();
      container->SetLayoutManager(std::make_unique<views::FillLayout>());

      if (params.loading_view->GetPreferredSize().IsEmpty()) {
        params.loading_view->SetPreferredSize(params.initial_size);
      }
      if (!params.loading_view->layer()) {
        params.loading_view->SetPaintToLayer();
      }

      if (params.loading_view->GetViewAccessibility().GetCachedRole() ==
              ax::mojom::Role::kNone ||
          params.loading_view->GetViewAccessibility().GetCachedRole() ==
              ax::mojom::Role::kUnknown) {
        params.loading_view->GetViewAccessibility().SetRole(
            ax::mojom::Role::kPane);
        if (!params.accessible_title.empty()) {
          params.loading_view->GetViewAccessibility().SetName(
              params.accessible_title, ax::mojom::NameFrom::kAttribute);
        } else {
          params.loading_view->GetViewAccessibility().SetName(
              "", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
        }
      }

      params.loading_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      if (focus_web_view_on_open_) {
        SetInitiallyFocusedView(params.loading_view.get());
      }

      loading_view_tracker_.SetView(params.loading_view.get());
      ax_lock_ = web_view_->DisconnectWebContentsAccessibility();

      container->AddChildView<views::View>(std::move(web_view));
      container->AddChildView(std::move(params.loading_view));
      SetContentsView(std::move(container));
    } else {
      if (focus_web_view_on_open_) {
        SetInitiallyFocusedView(web_view_);
      }
      SetContentsView(std::move(web_view));
    }
  }

  GlicModalDialogView(const GlicModalDialogView&) = delete;
  GlicModalDialogView& operator=(const GlicModalDialogView&) = delete;
  ~GlicModalDialogView() override {
    if (web_view_) {
      web_view_->clear_dialog_view();
    }
  }

  views::WebView* web_view() const { return web_view_; }

  bool is_loading() const { return loading_view_tracker_.view() != nullptr; }

  void set_pending_auto_resize(const gfx::Size& size) {
    pending_auto_resize_ = size;
  }

  tabs::TabDialogManager* tab_dialog_manager() const {
    tabs::TabInterface* tab = tab_handle_.Get();
    return tab ? tabs::TabDialogManager::From(tab) : nullptr;
  }

  void ShowContent() {
    views::View* loading_view = loading_view_tracker_.view();
    if (!loading_view && !ax_lock_) {
      return;
    }

    ax_lock_.reset();

    if (loading_view) {
      if (views::View* parent = loading_view->parent()) {
        parent->RemoveChildViewT(loading_view);
      }
    }

    if (pending_auto_resize_) {
      if (web_view_) {
        web_view_->SetPreferredSize(*pending_auto_resize_);
      }
      pending_auto_resize_.reset();
    }

    if (auto* manager = tab_dialog_manager()) {
      manager->UpdateModalDialogBounds();
    }

    if (focus_web_view_on_open_ && web_view_) {
      web_view_->RequestFocus();
    }
  }

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    if (focus_web_view_on_open_ && web_view_ && !is_loading()) {
      web_view_->RequestFocus();
    }
  }

  // views::WidgetDelegate:
  void OnWidgetInitialized() override {
    // When an accessible title is provided, observe the widget so we can
    // watch for visibility changes (`OnWidgetVisibilityChanged`) and
    // announce the title once the dialog actually becomes visible.
    if (!accessible_title_.empty() && GetWidget() &&
        !widget_observation_.IsObserving()) {
      widget_observation_.Observe(GetWidget());
    }
  }

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override {
    if (observed_view != web_view_) {
      return;
    }

    // Apply rounded corners to the BubbleFrameView and NativeViewHost to
    // prevent WebUI content or the dialog border from bleeding through the
    // dialog's rounded corners, even when window compositing is unavailable.
    const gfx::RoundedCornersF corner_radii(
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::ShapeContextTokens::kDialogRadius));
    if (views::BubbleFrameView* frame = GetBubbleFrameView()) {
      frame->SetRoundedCorners(corner_radii);
    }
    web_view_->holder()->SetNativeViewCornerRadii(corner_radii);

    if (views::View* loading_view = loading_view_tracker_.view()) {
      if (ui::Layer* layer = loading_view->layer()) {
        layer->SetRoundedCornerRadius(corner_radii);
        layer->SetIsFastRoundedCorner(true);
      }
    }
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    if (observed_view == web_view_) {
      Observe(nullptr);
      view_observation_.Reset();
      web_view_ = nullptr;
    }
  }

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    if (!visible || title_announced_ || accessible_title_.empty()) {
      return;
    }

    // Announce asynchronously: TabDialogManager shows and then activates the
    // widget synchronously, so posting a task lets the window finish becoming
    // the active window before the title is announced.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GlicModalDialogView::AnnounceAccessibleTitle,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    widget_observation_.Reset();
  }

 private:
  void AnnounceAccessibleTitle() {
    if (title_announced_ || accessible_title_.empty() || !GetWidget() ||
        !GetWidget()->IsVisible() || !GetWidget()->GetRootView()) {
      return;
    }

    title_announced_ = true;
    GetWidget()->GetRootView()->GetViewAccessibility().AnnounceAlert(
        accessible_title_);
  }

  const tabs::TabHandle tab_handle_;
  raw_ptr<GlicModalWebView> web_view_ = nullptr;
  bool focus_web_view_on_open_ = true;
  std::u16string accessible_title_;
  bool title_announced_ = false;
  views::ViewTracker loading_view_tracker_;
  std::unique_ptr<views::WebView::ScopedAxDisconnectLock> ax_lock_;
  std::optional<gfx::Size> pending_auto_resize_;
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<GlicModalDialogView> weak_ptr_factory_{this};
};

void GlicModalWebView::ResizeDueToAutoResize(content::WebContents* source,
                                             const gfx::Size& new_size) {
  if (suppress_first_auto_resize_ && !received_first_resize_) {
    received_first_resize_ = true;
    return;
  }

  if (dialog_view_ && dialog_view_->is_loading()) {
    dialog_view_->set_pending_auto_resize(new_size);
    return;
  }

  views::WebView::ResizeDueToAutoResize(source, new_size);
  tabs::TabInterface* tab = tab_handle_.Get();
  if (GetWidget() && tab) {
    if (auto* manager = tabs::TabDialogManager::From(tab)) {
      manager->UpdateModalDialogBounds();
    }
  }
}

class GlicModalDesktop : public GlicModal {
 public:
  static std::unique_ptr<GlicModal> CreateAndShow(tabs::TabInterface& tab,
                                                  Params params) {
    content::WebContents* tab_contents = tab.GetContents();
    if (!tab_contents || !tabs::TabDialogManager::From(&tab) ||
        !tab.CanShowModalUI()) {
      return nullptr;
    }

    Profile* profile =
        Profile::FromBrowserContext(tab_contents->GetBrowserContext());
    if (!profile) {
      return nullptr;
    }

    return std::make_unique<GlicModalDesktop>(profile, tab, std::move(params));
  }

  GlicModalDesktop(Profile* profile, tabs::TabInterface& tab, Params params)
      : tab_handle_(tab.GetHandle()),
        focus_web_view_on_open_(params.focus_web_view_on_open),
        on_closed_(std::move(params.on_closed)) {
    dialog_view_ = std::make_unique<GlicModalDialogView>(profile, tab, params);
    if (params.on_webui_created) {
      std::move(params.on_webui_created)
          .Run(dialog_view_->web_view()->GetWebContents());
    }
    if (params.on_available_size_changed) {
      params.on_available_size_changed.Run(params.max_size);
    }

    auto dialog_params = std::make_unique<tabs::TabDialogManager::Params>();
    dialog_params->close_on_navigate = false;
    tabs::TabDialogManager* tab_dialog_manager =
        tabs::TabDialogManager::From(&tab);
    dialog_widget_ = tab_dialog_manager->CreateAndShowDialog(
        dialog_view_.get(), std::move(dialog_params));
    if (focus_web_view_on_open_) {
      tab_dialog_manager->MaybeActivateDialog();
    }

    dialog_widget_->MakeCloseSynchronous(base::BindOnce(
        &GlicModalDesktop::OnWidgetClosed, weak_ptr_factory_.GetWeakPtr()));
  }

  ~GlicModalDesktop() override {
    weak_ptr_factory_.InvalidateWeakPtrs();
    on_closed_.Reset();
    auto widget = std::move(dialog_widget_);
    auto view = std::move(dialog_view_);
    widget.reset();
    view.reset();
  }

  void Close() override {
    OnWidgetClosed(views::Widget::ClosedReason::kUnspecified);
  }

  void Activate() override {
    tabs::TabInterface* tab = tab_handle_.Get();
    tabs::TabDialogManager* tab_dialog_manager =
        tab ? tabs::TabDialogManager::From(tab) : nullptr;
    if (!dialog_widget_ || !tab_dialog_manager) {
      return;
    }
    if (focus_web_view_on_open_) {
      tab_dialog_manager->MaybeActivateDialog();
    } else {
      tab_dialog_manager->UpdateDialogVisibility();
    }
  }

  void ShowContent() override {
    if (dialog_view_) {
      dialog_view_->ShowContent();
    }
  }

  content::WebContents* GetWebContents() const override {
    views::WebView* web_view =
        dialog_view_ ? dialog_view_->web_view() : nullptr;
    return web_view ? web_view->GetWebContents() : nullptr;
  }

 private:
  void OnWidgetClosed(views::Widget::ClosedReason reason) {
    if (!dialog_widget_) {
      return;
    }
    weak_ptr_factory_.InvalidateWeakPtrs();
    auto widget = std::move(dialog_widget_);
    auto view = std::move(dialog_view_);
    auto on_closed = std::move(on_closed_);
    widget.reset();
    view.reset();
    if (on_closed) {
      std::move(on_closed).Run();
    }
  }

  const tabs::TabHandle tab_handle_;
  bool focus_web_view_on_open_ = true;
  std::unique_ptr<GlicModalDialogView> dialog_view_;
  std::unique_ptr<views::Widget> dialog_widget_;
  base::OnceClosure on_closed_;
  base::WeakPtrFactory<GlicModalDesktop> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<GlicModal> GlicModal::Show(tabs::TabInterface& tab,
                                           Params params) {
  return GlicModalDesktop::CreateAndShow(tab, std::move(params));
}

}  // namespace glic
