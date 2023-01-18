// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_gl_thread.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/ui_factory.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/sounds_manager_audio_delegate.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gl/android/surface_texture.h"

namespace vr {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    const UiInitialState& ui_initial_state,
    bool reprojected_rendering,
    bool daydream_support,
    base::WaitableEvent* gl_surface_created_event,
    base::OnceCallback<gfx::AcceleratedWidget()> surface_callback)
    : base::android::JavaHandlerThread("VrShellGL"),
      weak_vr_shell_(weak_vr_shell),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      gvr_api_(gvr::GvrApi::WrapNonOwned(gvr_api)),
      factory_params_(std::make_unique<BrowserRendererFactory::Params>(
          gvr_api_.get(),
          ui_initial_state,
          reprojected_rendering,
          gvr_api_->GetViewerType() ==
              gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD,
          gl_surface_created_event,
          std::move(surface_callback))) {
  MetricsUtilAndroid::LogVrViewerType(gvr_api_->GetViewerType());
}

VrGLThread::~VrGLThread() {
  Stop();
}

base::WeakPtr<BrowserRenderer> VrGLThread::GetBrowserRenderer() {
  return browser_renderer_->GetWeakPtr();
}

void VrGLThread::Init() {
  ui_factory_ = CreateUiFactory();
  browser_renderer_ = BrowserRendererFactory::Create(
      this, ui_factory_.get(), std::move(factory_params_));
  weak_browser_ui_ = browser_renderer_->GetBrowserUiWeakPtr();
}

void VrGLThread::CleanUp() {
  browser_renderer_.reset();
}

void VrGLThread::GvrDelegateReady() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::GvrDelegateReady, weak_vr_shell_));
}

void VrGLThread::SendRequestPresentReply(device::mojom::XRSessionPtr session) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::SendRequestPresentReply,
                                weak_vr_shell_, std::move(session)));
}

void VrGLThread::ForceExitVr() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ForceExitVr, weak_vr_shell_));
}

void VrGLThread::ExitPresent() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ExitPresent, weak_vr_shell_));
  browser_renderer_->OnExitPresent();
}

void VrGLThread::ExitFullscreen() {}

void VrGLThread::Navigate(GURL gurl, NavigationMethod method) {}

void VrGLThread::NavigateBack() {}

void VrGLThread::NavigateForward() {}

void VrGLThread::ReloadTab() {}

void VrGLThread::OpenNewTab(bool incognito) {}

void VrGLThread::OpenBookmarks() {}

void VrGLThread::OpenRecentTabs() {}

void VrGLThread::OpenHistory() {}

void VrGLThread::OpenDownloads() {}

void VrGLThread::OpenShare() {}

void VrGLThread::OpenSettings() {}

void VrGLThread::CloseAllIncognitoTabs() {}

void VrGLThread::OpenFeedback() {}

void VrGLThread::CloseHostedDialog() {}

void VrGLThread::ToggleCardboardGamepad(bool enabled) {}

void VrGLThread::OnUnsupportedMode(UiUnsupportedMode mode) {}

void VrGLThread::OnExitVrPromptResult(ExitVrPromptChoice choice,
                                      UiUnsupportedMode reason) {}

void VrGLThread::SetVoiceSearchActive(bool active) {}

void VrGLThread::StartAutocomplete(const AutocompleteRequest& request) {}

void VrGLThread::StopAutocomplete() {}

void VrGLThread::ShowPageInfo() {}

void VrGLThread::SetFullscreen(bool enabled) {}

void VrGLThread::SetIncognito(bool incognito) {}

void VrGLThread::SetHistoryButtonsEnabled(bool can_go_back,
                                          bool can_go_forward) {}

void VrGLThread::SetLoadProgress(float progress) {}

void VrGLThread::SetLoading(bool loading) {}

void VrGLThread::SetLocationBarState(const LocationBarState& state) {}

void VrGLThread::SetWebVrMode(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetWebVrMode,
                                         weak_browser_ui_, enabled));
}

void VrGLThread::SetCapturingState(
    const CapturingStateModel& active_capturing,
    const CapturingStateModel& background_capturing,
    const CapturingStateModel& potential_capturing) {}

void VrGLThread::ShowExitVrPrompt(UiUnsupportedMode reason) {}

void VrGLThread::SetHasOrCanRequestRecordAudioPermission(
    bool const has_or_can_request_record_audio) {}

void VrGLThread::SetSpeechRecognitionEnabled(bool enabled) {}

void VrGLThread::SetRecognitionResult(const std::u16string& result) {}

void VrGLThread::OnSpeechRecognitionStateChanged(int new_state) {}

void VrGLThread::SetOmniboxSuggestions(
    std::vector<OmniboxSuggestion> suggestions) {}

void VrGLThread::OnAssetsLoaded(AssetsLoadStatus status,
                                std::unique_ptr<Assets> assets,
                                const base::Version& component_version) {}

void VrGLThread::OnAssetsUnavailable() {}

void VrGLThread::WaitForAssets() {}

void VrGLThread::SetRegularTabsOpen(bool open) {}

void VrGLThread::SetIncognitoTabsOpen(bool open) {}

void VrGLThread::SetOverlayTextureEmpty(bool empty) {}

void VrGLThread::ShowSoftInput(bool show) {}

void VrGLThread::UpdateWebInputIndices(int selection_start,
                                       int selection_end,
                                       int composition_start,
                                       int composition_end) {}

void VrGLThread::SetDialogLocation(float x, float y) {}

void VrGLThread::SetDialogFloating(bool floating) {}

void VrGLThread::ShowPlatformToast(const std::u16string& text) {}

void VrGLThread::CancelPlatformToast() {}

void VrGLThread::SetVisibleExternalPromptNotification(
    ExternalPromptNotificationType prompt) {
  // Not reached on Android.
  NOTREACHED();
}

void VrGLThread::ReportUiOperationResultForTesting(
    const UiTestOperationType& action_type,
    const UiTestOperationResult& result) {}

bool VrGLThread::OnMainThread() const {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

bool VrGLThread::OnGlThread() const {
  return task_runner()->BelongsToCurrentThread();
}

}  // namespace vr
