// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_floating_ui.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/application_hotkey_delegate.h"
#include "chrome/browser/glic/widget/glic_inactive_floating_ui.h"
#include "chrome/browser/glic/widget/glic_panel_hotkey_delegate.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

namespace {
BASE_FEATURE(kGlicFloatingUiReattachment, base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

// static
gfx::Size GlicFloatingUi::GetDefaultSize() {
  return {features::kGlicMultiInstanceFloatyWidth.Get(),
          features::kGlicMultiInstanceFloatyHeight.Get()};
}
// end static

GlicFloatingUi::GlicFloatingUi(Profile* profile,
                               gfx::Rect initial_bounds,
                               tabs::TabHandle source_tab,
                               GlicUiEmbedder::Delegate& delegate,
                               GlicInstanceMetrics& instance_metrics)
    : profile_(profile),
      delegate_(delegate),
      instance_metrics_(instance_metrics),
      source_tab_(source_tab) {
  if (auto* helper = GlicInstanceHelper::From(source_tab_.Get())) {
    source_tab_destruction_subscription_ =
        helper->SubscribeToDestruction(base::BindRepeating(
            &GlicFloatingUi::OnSourceTabDestroyed, base::Unretained(this)));
  }
  application_hotkey_manager_ =
      MakeApplicationHotkeyManager(weak_ptr_factory_.GetWeakPtr());
  glic_panel_hotkey_manager_ =
      MakeGlicWindowHotkeyManager(weak_ptr_factory_.GetWeakPtr());
  CreateAndSetupWidget(initial_bounds);
  panel_state_.kind = mojom::PanelStateKind::kDetached;
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  tracker->OnPictureInPictureWidgetOpened(glic_widget_.get());
}

GlicFloatingUi::~GlicFloatingUi() {
  GlicProfileManager::GetInstance()->SetCurrentDetachedGlic(nullptr);
  ClearWebContentsDelegate();
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  tracker->RemovePictureInPictureWidget(glic_widget_.get());
  if (auto* glic_view = GetGlicView()) {
    glic_view->SetWebContents(nullptr);
  }
}

void GlicFloatingUi::OnClientReady() {
  instance_metrics_->OnClientReady(GlicInstanceMetrics::EmbedderType::kFloaty);
}

Host::EmbedderDelegate* GlicFloatingUi::GetHostEmbedderDelegate() {
  return this;
}

mojom::PanelState GlicFloatingUi::GetPanelState() const {
  return panel_state_;
}

gfx::Size GlicFloatingUi::GetPanelSize() {
  if (auto* glic_widget = GetGlicWidget()) {
    return glic_widget->GetSize();
  }
  return gfx::Size();
}

GlicWidget* GlicFloatingUi::GetGlicWidget() const {
  return glic_widget_.get();
}

GlicView* GlicFloatingUi::GetGlicView() const {
  if (auto* glic_widget = GetGlicWidget()) {
    return glic_widget->GetGlicView();
  }
  return nullptr;
}

void GlicFloatingUi::CreateAndSetupWidget(gfx::Rect initial_bounds) {
  auto glic_view =
      std::make_unique<GlicView>(profile_, initial_bounds.size(),
                                 glic_panel_hotkey_manager_->GetWeakPtr());
  glic_delegate_ =
      GlicWidget::CreateWidgetDelegate(std::move(glic_view), user_resizable_);
  glic_widget_ = GlicWidget::Create(glic_delegate_.get(), profile_,
                                    initial_bounds, user_resizable_);

  // TODO: Setup AccessibilityText.
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(true);
  GetGlicWidget()->SetVisibleOnAllWorkspaces(true);
  GetGlicWidget()->SetCanAppearInExistingFullscreenSpaces(true);
#endif

  glic_window_animator_ = std::make_unique<GlicWindowAnimator>(
      glic_widget_->GetWeakPtr(),
      base::BindRepeating(&GlicFloatingUi::MaybeSetWidgetCanResize,
                          weak_ptr_factory_.GetWeakPtr()));
  window_event_observer_ = std::make_unique<GlicWindowEventObserver>(
      glic_widget_->GetWeakPtr(), this);
  glic_widget_observation_.Observe(GetGlicWidget());
}

void GlicFloatingUi::Resize(const gfx::Size& size,
                            base::TimeDelta duration,
                            base::OnceClosure callback) {
  if (!user_resizing_ && glic_window_animator_ && IsShowing()) {
    glic_window_animator_->AnimateSize(
        GlicWidget::ClampSize(size, GetGlicWidget()), duration,
        std::move(callback));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

void GlicFloatingUi::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  if (auto* glic_view = GetGlicView()) {
    glic_view->SetDraggableAreas(draggable_areas);
  }
}

GlicWindowAnimator* GlicFloatingUi::window_animator() {
  return glic_window_animator_.get();
}

void GlicFloatingUi::OnDragComplete() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::FocusIfOpen() {
  if (!IsShowing() || HasFocus()) {
    return;
  }
  GetGlicWidget()->Activate();
  GetGlicView()->GetWebContents()->Focus();
}

bool GlicFloatingUi::HasFocus() {
  return IsShowing() && GetGlicWidget()->IsActive();
}

bool GlicFloatingUi::ActivateBrowser() {
  if (auto* const last_active_bwi =
          GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
    last_active_bwi->GetWindow()->Activate();
    return true;
  }
  return false;
}

void GlicFloatingUi::ShowTitleBarContextMenuAt(gfx::Point event_loc) {
#if BUILDFLAG(IS_WIN)
  views::View::ConvertPointToScreen(GetGlicView(), &event_loc);
  event_loc = display::win::GetScreenWin()->DIPToScreenPoint(event_loc);
  views::ShowSystemMenuAtScreenPixelLocation(views::HWNDForView(GetGlicView()),
                                             event_loc);
#endif  // BUILDFLAG(IS_WIN)
}

void GlicFloatingUi::EnableDragResize(bool enabled) {
  user_resizable_ = enabled;

  MaybeSetWidgetCanResize();
  GetGlicView()->UpdateBackgroundColor();
  glic_window_animator_->MaybeAnimateToTargetSize();
}

void GlicFloatingUi::MaybeSetWidgetCanResize() {
  // During teardown, the widget's delegate is null. Just return early.
  if (!GetGlicWidget()->widget_delegate()) {
    return;
  }

  if (GetGlicWidget()->widget_delegate()->CanResize() == user_resizable_ ||
      glic_window_animator_->IsAnimating()) {
    // If the resize state is already correct or the widget is animating do not
    // update the resize state.
    return;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows when resize is enabled there is an invisible border added
  // around the client area. We need to make the widget larger or smaller to
  // keep the visible client area the same size.
  gfx::Rect previous_client_bounds =
      GetGlicWidget()->GetClientAreaBoundsInScreen();
#endif  // BUILDFLAG(IS_WIN)

  // Update resize state on widget delegate.
  GetGlicWidget()->widget_delegate()->SetCanResize(user_resizable_);

#if BUILDFLAG(IS_WIN)
  if (user_resizable_) {
    // Resizable so the widget area is larger than the client area.
    gfx::Rect new_widget_bounds =
        GetGlicWidget()->VisibleToWidgetBounds(previous_client_bounds);
    GetGlicWidget()->SetBoundsConstrained(new_widget_bounds);
  } else {
    // Not resizable so the client and widget areas are the same.
    GetGlicWidget()->SetBoundsConstrained(previous_client_bounds);
  }
#endif  // BUILDFLAG(IS_WIN)
}

void GlicFloatingUi::OnSourceTabDestroyed(tabs::TabInterface* tab,
                                          const InstanceId& instance_id) {
  FloatingPanelCanAttachChanged(false);
}

void GlicFloatingUi::FloatingPanelCanAttachChanged(bool can_attach) {
  if (!base::FeatureList::IsEnabled(kGlicFloatingUiReattachment)) {
    return;
  }
  delegate_->host().FloatingPanelCanAttachChanged(can_attach);
}

void GlicFloatingUi::Attach() {
  if (!base::FeatureList::IsEnabled(kGlicFloatingUiReattachment)) {
    return;
  }
  if (!source_tab_.Get()) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  delegate_->Attach(*source_tab_.Get());
}

void GlicFloatingUi::Detach() {
  // Floaty UI is already detached.
  LOG(WARNING) << "GlicFloatingUi: Detach() called while already detached.";
}

void GlicFloatingUi::SetMinimumWidgetSize(const gfx::Size& size) {
  GetGlicWidget()->SetMinimumSize(size);
}

bool GlicFloatingUi::IsShowing() const {
  return glic_widget_ != nullptr;
}

void GlicFloatingUi::Show(const ShowOptions& options) {
  FloatingPanelCanAttachChanged(source_tab_.Get() != nullptr);
  instance_metrics_->OnShowInFloaty(options);
  GlicProfileManager::GetInstance()->SetCurrentDetachedGlic(profile_);
  GetGlicWidget()->Show();
  GetGlicView()->SetWebContents(delegate_->host().webui_contents());
  GetGlicView()->UpdateBackgroundColor();
  application_hotkey_manager_->InitializeAccelerators();
  glic_panel_hotkey_manager_->InitializeAccelerators();
  // TODO: Set up manual resize.
  window_event_observer_->SetDraggingAreasAndWatchForMouseEvents();
  // Add capability to show web modal dialogs (e.g. Data Controls Dialogs for
  // enterprise users) via constrained_window APIs.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      delegate_->host().webui_contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      delegate_->host().webui_contents())
      ->SetDelegate(this);
}

void GlicFloatingUi::Close() {
  instance_metrics_->OnFloatyClosed();
  if (IsShowing()) {
    modal_dialog_host_observers_.Notify(
        &web_modal::ModalDialogHostObserver::OnHostDestroying);
  }
  ClearWebContentsDelegate();
  if (screenshot_capturer_) {
    screenshot_capturer_->CloseScreenPicker();
  }
  FloatingPanelCanAttachChanged(false);
  window_event_observer_.reset();
  glic_window_animator_.reset();
  glic_widget_observation_.Reset();
  glic_widget_.reset();
  glic_delegate_.reset();
  user_resizable_ = false;
  // NOTE: `this` will be destroyed after this call.
  delegate_->WillCloseFor(FloatingEmbedderKey{});
}

void GlicFloatingUi::ClearWebContentsDelegate() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    auto* dialog_manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
    if (dialog_manager->delegate() == this) {
      dialog_manager->SetDelegate(nullptr);
    }
  }
}

void GlicFloatingUi::ClosePanel() {
  Close();
}

void GlicFloatingUi::Focus() {
  if (!IsShowing()) {
    return;
  }
  GetGlicWidget()->Activate();
  if (auto* web_contents = delegate_->host().webui_contents()) {
    web_contents->Focus();
  }
}

void GlicFloatingUi::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  delegate_->OnEmbedderWindowActivationChanged(active);
}

void GlicFloatingUi::OnWidgetDestroyed(views::Widget* widget) {
  // This is used to handle the case where the native window is closed
  // directly (e.g., Windows context menu close on the title bar).
  // Conceptually this should synchronously call Close(), but the Widget
  // implementation currently does not support this.
  if (GetGlicWidget() == widget) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlicFloatingUi::Close, weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicFloatingUi::OnWidgetBoundsChanged(views::Widget* widget,
                                           const gfx::Rect& new_bounds) {
  modal_dialog_host_observers_.Notify(
      &web_modal::ModalDialogHostObserver::OnPositionRequiresUpdate);
}

void GlicFloatingUi::OnWidgetUserResizeStarted() {
  user_resizing_ = true;
  instance_metrics_->OnUserResizeStarted(GetPanelSize());
  if (GlicWebClientAccess* client = delegate_->host().GetPrimaryWebClient()) {
    client->ManualResizeChanged(true);
  }
}

void GlicFloatingUi::OnWidgetUserResizeEnded() {
  instance_metrics_->OnUserResizeEnded(GetPanelSize());
  if (GlicWebClientAccess* client = delegate_->host().GetPrimaryWebClient()) {
    client->ManualResizeChanged(false);
  }

  if (GetGlicView()) {
    GetGlicView()->UpdatePrimaryDraggableAreaOnResize();
  }

  glic_window_animator_->ResetLastTargetSize();
  user_resizing_ = false;
}

// web_modal::WebContentsModalDialogManagerDelegate
// web_modal::WebContentsModalDialogHost
web_modal::WebContentsModalDialogHost*
GlicFloatingUi::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return this;
}

gfx::Size GlicFloatingUi::GetMaximumDialogSize() {
  return GetGlicWidget()->GetClientAreaBoundsInScreen().size();
}

gfx::NativeView GlicFloatingUi::GetHostView() const {
  return GetGlicWidget()->GetNativeView();
}

gfx::Point GlicFloatingUi::GetDialogPosition(const gfx::Size& dialog_size) {
  gfx::Rect client_area_bounds = GetGlicWidget()->GetClientAreaBoundsInScreen();
  return gfx::Point((client_area_bounds.width() - dialog_size.width()) / 2, 0);
}

bool GlicFloatingUi::ShouldConstrainDialogBoundsByHost() {
  // Allows web modal dialogs to extend beyond the boundary of glic window.
  // These web modals are usually larger than the glic window.
  return false;
}

void GlicFloatingUi::AddObserver(web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.AddObserver(observer);
}

void GlicFloatingUi::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observers_.RemoveObserver(observer);
}

std::unique_ptr<GlicUiEmbedder> GlicFloatingUi::CreateInactiveEmbedder() const {
  return GlicInactiveFloatingUi::From(*this);
}

base::WeakPtr<views::View> GlicFloatingUi::GetView() {
  if (auto* glic_view = GetGlicView()) {
    return glic_view->GetWeakPtr();
  }
  return nullptr;
}

void GlicFloatingUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  // NOTE: `this` may be destroyed after this call.
  delegate_->SwitchConversation(
      ShowOptions::ForFloating(GetGlicWidget()->GetWindowBoundsInScreen()),
      std::move(info), std::move(callback));
}

void GlicFloatingUi::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (!screenshot_capturer_) {
    screenshot_capturer_ = std::make_unique<GlicScreenshotCapturer>();
  }
  screenshot_capturer_->CaptureScreenshot(GetGlicWidget()->GetNativeWindow(),
                                          std::move(callback));
}

std::string GlicFloatingUi::DescribeForTesting() {
  return "FloatingUi";
}

}  // namespace glic
