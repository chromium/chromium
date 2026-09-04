// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/glic/android/glic_helper_android.h"
#include "chrome/browser/glic/common/panel_focus_dependent_hotkey_manager.h"
#include "chrome/browser/glic/common/panel_visibility_dependent_hotkey_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/conversions.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"
#include "chrome/browser/glic/widget/web_contents_delegate_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/chrome_features.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/android/accelerator_manager_android.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/print_composite_client.h"
#endif

namespace glic {

namespace {

void OnMediaAccessPermissionResult(
    base::WeakPtr<content::WebContents> web_contents,
    blink::mojom::MediaStreamType audio_type,
    content::MediaResponseCallback callback,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<content::MediaStreamUI> ui) {
  if (result != blink::mojom::MediaStreamRequestResult::OK &&
      blink::IsAudioInputMediaType(audio_type)) {
    if (web_contents) {
      ShowMicDisabledSnackbar(web_contents->GetTopLevelNativeWindow());
    }
  }
  std::move(callback).Run(stream_devices_set, result, std::move(ui));
}

}  // namespace

GlicSidePanelUi::GlicSidePanelUi(Profile* profile,
                                 base::WeakPtr<tabs::TabInterface> tab,
                                 GlicUiEmbedder::Delegate& delegate,
                                 GlicInstanceMetrics& instance_metrics)
    : web_contents_delegate_android::WebContentsDelegateAndroid(
          base::android::AttachCurrentThread(),
          /*obj=*/nullptr),  // Null peer is safely handled and falls back to
                             // base behavior.
      tab_(tab),

      delegate_(delegate),

      instance_metrics_(instance_metrics),
      profile_(profile) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }

  panel_visibility_dependent_hotkey_manager_ =
      std::make_unique<PanelVisibilityDependentHotkeyManager>(
          profile_, weak_ptr_factory_.GetWeakPtr());
  panel_focus_dependent_hotkey_manager_ =
      std::make_unique<PanelFocusDependentHotkeyManager>(
          weak_ptr_factory_.GetWeakPtr());

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddStateCallback(
          base::BindRepeating(&GlicSidePanelUi::SidePanelStateChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  browser_observation_.Observe(GlobalBrowserCollection::GetInstance());
  if (auto* browser_window = tab_->GetBrowserWindowInterface()) {
    delegate_->OnEmbedderWindowActivationChanged(
        browser_window->GetWindow()->IsActive());
  }

  // In NoWebview mode, PrivilegedWebContents owns the WebContentsDelegate.
  // We attach as its EmbedderDelegate to receive non-security callbacks
  // (such as zoom changes and keyboard events).
  // TODO(crbug.com/534807813): Plumb remaining required delegate callbacks via
  // PrivilegedWebContents APIs instead of setting the delegate directly.
  content::WebContents* web_contents = delegate_->host().webui_contents();
  SetWebContentsDelegate(web_contents, /*delegate=*/this);

  glic_side_panel_coordinator->SetWebContents(web_contents);

  host_observation_.Observe(&delegate_->host());
  panel_state_.kind = mojom::PanelStateKind::kAttached;
}

GlicSidePanelUi::~GlicSidePanelUi() {
  // Explicitly reset the hotkey managers to destroy their registrations and
  // unregister from the WindowAndroid while `weak_ptr_factory_` (and any
  // `panel_` weak pointers) is still valid.
  panel_focus_dependent_hotkey_manager_.reset();
  panel_visibility_dependent_hotkey_manager_.reset();
  content::WebContents* web_contents = delegate_->host().webui_contents();
  SetWebContentsDelegate(web_contents, /*delegate=*/nullptr,
                         /*expected_delegate=*/this);
}

Host::EmbedderDelegate* GlicSidePanelUi::GetHostEmbedderDelegate() {
  return this;
}

void GlicSidePanelUi::Show(const ShowOptions& options) {
  instance_metrics_->OnShowInSidePanel(tab_.get());
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  panel_state_.kind = mojom::PanelStateKind::kAttached;
  delegate_->NotifyPanelStateChanged();
  delegate_->host().FloatingPanelCanAttachChanged(false);
  panel_visibility_dependent_hotkey_manager_->InitializeAccelerators();
  panel_focus_dependent_hotkey_manager_->InitializeAccelerators();

  glic_side_panel_coordinator->Show(ConvertToCoordinatorShowOptions(
      options, glic_side_panel_coordinator->SupportsPeek()));
}

void GlicSidePanelUi::Close(const CloseOptions& options) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  glic_side_panel_coordinator->Close(options);
}

void GlicSidePanelUi::Focus() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    web_contents->Focus();
  }
}

bool GlicSidePanelUi::HasFocus() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    if (auto* view = web_contents->GetRenderWidgetHostView()) {
      return view->HasFocus();
    }
  }
  return false;
}

mojom::PanelState GlicSidePanelUi::GetPanelState() const {
  return panel_state_;
}

gfx::Size GlicSidePanelUi::GetPanelSize() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    return web_contents->GetContainerBounds().size();
  }
  return gfx::Size();
}

std::string GlicSidePanelUi::DescribeForTesting() {
  return base::StrCat({"SidePanelUi for tab ",
                       base::NumberToString(tab_->GetHandle().raw_value())});
}

std::unique_ptr<GlicUiEmbedder> GlicSidePanelUi::CreateInactiveEmbedder()
    const {
  return GlicInactiveSidePanelUi::CreateForVisibleTab(tab_, *delegate_);
}

void GlicSidePanelUi::Attach() {
  // The Side Panel Ui is already attached, do nothing.
}

void GlicSidePanelUi::Detach() {
  if (!tab_) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  delegate_->Detach(*tab_);
}

void GlicSidePanelUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  // NOTE: `this` may be destroyed after this call.
  delegate_->SwitchConversation(ShowOptions::ForSidePanel(*tab_),
                                std::move(info), std::move(callback));
}

void GlicSidePanelUi::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (!tab_) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (!screenshot_capturer_) {
    screenshot_capturer_ = GlicScreenshotCapturer::Create();
  }
  auto* browser_window = tab_->GetBrowserWindowInterface();
  CHECK(browser_window);
  screenshot_capturer_->CaptureScreenshot(
      browser_window->GetWindow()->GetNativeWindow(), std::move(callback));
}

bool GlicSidePanelUi::IsShowing() const {
  return GlicSidePanelCoordinator::IsShowing(tab_.get());
}

bool GlicSidePanelUi::IsShowingOrBackgrounded() const {
  return GlicSidePanelCoordinator::IsShowingOrBackgrounded(tab_.get());
}

void GlicSidePanelUi::ClosePanel() {
  Close(CloseOptions());
}

void GlicSidePanelUi::OnReload() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (glic_side_panel_coordinator) {
    glic_side_panel_coordinator->SetWebContents(
        delegate_->host().webui_contents());
  }
}

void GlicSidePanelUi::ActiveWebContentsChanged(
    content::WebContents* new_contents) {
  if (auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator()) {
    content::WebContents* old_contents = delegate_->host().webui_contents();
    if (old_contents && old_contents != new_contents) {
      SetWebContentsDelegate(old_contents, /*delegate=*/nullptr,
                             /*expected_delegate=*/this);
    }
    SetWebContentsDelegate(new_contents, /*delegate=*/this);
    glic_side_panel_coordinator->SetWebContents(new_contents);
  }
}

void GlicSidePanelUi::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (tab_ && tab_->GetBrowserWindowInterface() == browser) {
    delegate_->OnEmbedderWindowActivationChanged(true);
  }
}

void GlicSidePanelUi::OnBrowserDeactivated(BrowserWindowInterface* browser) {
  if (tab_ && tab_->GetBrowserWindowInterface() == browser) {
    delegate_->OnEmbedderWindowActivationChanged(false);
  }
}

namespace {

BASE_FEATURE(kGlicEscapeHandling, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUnmodifiedEscapeKeyDown(const input::NativeWebKeyboardEvent& event) {
  return event.windows_key_code == ui::VKEY_ESCAPE &&
         (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown ||
          event.GetType() == input::NativeWebKeyboardEvent::Type::kKeyDown) &&
         !(event.GetModifiers() & blink::WebInputEvent::kKeyModifiers);
}

EmbedderCloseReason MapStateToCloseReason(
    GlicSidePanelCoordinator::State state) {
  switch (state) {
    case GlicSidePanelCoordinator::State::kBackgrounded:
      return EmbedderCloseReason::kBackgrounded;
    case GlicSidePanelCoordinator::State::kPeek:
      return EmbedderCloseReason::kPeek;
    case GlicSidePanelCoordinator::State::kClosed:
      return EmbedderCloseReason::kExplicitlyClosed;
    case GlicSidePanelCoordinator::State::kShown:
      NOTREACHED()
          << "This mapping is only called when the state is not kShown";
  }
}
}  // namespace

void GlicSidePanelUi::SidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  if (state != GlicSidePanelCoordinator::State::kShown && tab_) {
    GlicInstanceMetrics::CloseReason reason =
        state == GlicSidePanelCoordinator::State::kBackgrounded
            ? GlicInstanceMetrics::CloseReason::kTabSwitched
            : GlicInstanceMetrics::CloseReason::kExplicitlyClosed;
    instance_metrics_->OnSidePanelClosed(tab_.get(), reason);
    panel_state_.kind = mojom::PanelStateKind::kHidden;
    delegate_->NotifyPanelStateChanged();

    // NOTE: `this` will be destroyed after this call.
    delegate_->DidCloseFor(SidePanelEmbedderKey{*tab_},
                           MapStateToCloseReason(state));
  }
}

GlicSidePanelCoordinator* GlicSidePanelUi::GetGlicSidePanelCoordinator() const {
  return GlicSidePanelCoordinator::GetForTab(tab_.get());
}

bool GlicSidePanelUi::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::DragOperationsMask operations_allowed) {
  if (!base::FeatureList::IsEnabled(features::kGlicDragAndDropFileUpload) ||
      !base::FeatureList::IsEnabled(
          features::kGlicDragAndDropFileUploadAndroid)) {
    return false;
  }
  return !data.filenames.empty() || !data.file_system_files.empty();
}

void GlicSidePanelUi::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request,
      base::BindOnce(&OnMediaAccessPermissionResult,
                     web_contents ? web_contents->GetWeakPtr() : nullptr,
                     request.audio_type, std::move(callback)),
      nullptr);
}

bool GlicSidePanelUi::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

void GlicSidePanelUi::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void GlicSidePanelUi::PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const {
#if BUILDFLAG(ENABLE_PRINTING)
  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client) {
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
  }
#endif
}

void GlicSidePanelUi::FocusIfOpen() {
  if (IsShowing()) {
    Focus();
  }
}

bool GlicSidePanelUi::ActivateBrowser() {
  if (!tab_) {
    return false;
  }
  tab_->GetContents()->Focus();
  return true;
}

void GlicSidePanelUi::Zoom(mojom::ZoomAction zoom_action, ZoomSource source) {
  delegate_->host().Zoom(zoom_action, source);
}

BrowserWindowInterface* GlicSidePanelUi::GetBrowserWindowInterface() {
  return tab_ ? tab_->GetBrowserWindowInterface() : nullptr;
}

// TODO(crbug.com/542609750): Remove once unified keyboard handling is
// supported on Android.
content::KeyboardEventProcessingResult GlicSidePanelUi::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!base::FeatureList::IsEnabled(kGlicEscapeHandling)) {
    return web_contents_delegate_android::WebContentsDelegateAndroid::
        PreHandleKeyboardEvent(source, event);
  }

  if (IsUnmodifiedEscapeKeyDown(event)) {
    if (tab_ && tab_->GetContents()) {
      if (auto* delegate = tab_->GetContents()->GetDelegate()) {
        auto result =
            delegate->PreHandleKeyboardEvent(tab_->GetContents(), event);
        if (result != content::KeyboardEventProcessingResult::NOT_HANDLED) {
          // If the primary tab handled Escape (e.g. exiting fullscreen mode or
          // pointer lock), also close the side panel.
          Close(CloseOptions());
          return result;
        }
      }
    }
  }
  return web_contents_delegate_android::WebContentsDelegateAndroid::
      PreHandleKeyboardEvent(source, event);
}

// TODO(crbug.com/542609750): Remove once unified keyboard handling is
// supported on Android.
bool GlicSidePanelUi::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!base::FeatureList::IsEnabled(kGlicEscapeHandling)) {
    return web_contents_delegate_android::WebContentsDelegateAndroid::
        HandleKeyboardEvent(source, event);
  }

  if (IsUnmodifiedEscapeKeyDown(event)) {
    Close(CloseOptions());
    return true;
  }
  return web_contents_delegate_android::WebContentsDelegateAndroid::
      HandleKeyboardEvent(source, event);
}

void GlicSidePanelUi::ContentsZoomChange(bool zoom_in) {
  delegate_->host().Zoom(
      zoom_in ? mojom::ZoomAction::kZoomIn : mojom::ZoomAction::kZoomOut,
      ZoomSource::kScroll);
}

}  // namespace glic
