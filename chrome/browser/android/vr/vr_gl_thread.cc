// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_gl_thread.h"

#include <utility>

#include "base/strings/string16.h"
#include "base/version.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/vr_input_connection.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/sounds_manager_audio_delegate.h"
#include "chrome/browser/vr/ui_factory.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/common/chrome_features.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    const UiInitialState& ui_initial_state,
    bool reprojected_rendering,
    bool daydream_support,
    bool pause_content,
    bool low_density,
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
          pause_content,
          low_density,
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

void VrGLThread::SetInputConnection(VrInputConnection* input_connection) {
  DCHECK(OnGlThread());
  input_connection_ = input_connection;
}

void VrGLThread::Init() {
  ui_factory_ = std::make_unique<UiFactory>();
  browser_renderer_ = BrowserRendererFactory::Create(
      this, ui_factory_.get(), std::move(factory_params_));
  weak_browser_ui_ = browser_renderer_->GetBrowserUiWeakPtr();
}

void VrGLThread::CleanUp() {
  browser_renderer_.reset();
}

void VrGLThread::ContentSurfaceCreated(jobject surface,
                                       gl::SurfaceTexture* texture) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ContentSurfaceCreated, weak_vr_shell_,
                                surface, base::Unretained(texture)));
}

void VrGLThread::ContentOverlaySurfaceCreated(jobject surface,
                                              gl::SurfaceTexture* texture) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::ContentOverlaySurfaceCreated, weak_vr_shell_,
                     surface, base::Unretained(texture)));
}

void VrGLThread::DialogSurfaceCreated(jobject surface,
                                      gl::SurfaceTexture* texture) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::DialogSurfaceCreated, weak_vr_shell_,
                                surface, base::Unretained(texture)));
}

void VrGLThread::GvrDelegateReady(gvr::ViewerType viewer_type) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::GvrDelegateReady, weak_vr_shell_, viewer_type));
}

void VrGLThread::SendRequestPresentReply(device::mojom::XRSessionPtr session) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::SendRequestPresentReply,
                                weak_vr_shell_, std::move(session)));
}

void VrGLThread::UpdateGamepadData(device::GvrGamepadData pad) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::UpdateGamepadData, weak_vr_shell_, pad));
}

void VrGLThread::ForwardEventToContent(std::unique_ptr<InputEvent> event,
                                       int content_id) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ProcessContentGesture, weak_vr_shell_,
                                base::Passed(std::move(event)), content_id));
}

void VrGLThread::ClearFocusedElement() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ClearFocusedElement, weak_vr_shell_));
}

void VrGLThread::OnWebInputEdited(const TextEdits& edits) {
  DCHECK(OnGlThread());
  DCHECK(input_connection_);
  input_connection_->OnKeyboardEdit(edits);
}

void VrGLThread::SubmitWebInput() {
  DCHECK(OnGlThread());
  DCHECK(input_connection_);
  input_connection_->SubmitInput();
}

void VrGLThread::RequestWebInputText(TextStateUpdateCallback callback) {
  DCHECK(OnGlThread());
  DCHECK(input_connection_);
  input_connection_->RequestTextState(std::move(callback));
}

void VrGLThread::ForwardEventToPlatformUi(std::unique_ptr<InputEvent> event) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ProcessDialogGesture, weak_vr_shell_,
                                base::Passed(std::move(event))));
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

void VrGLThread::ExitFullscreen() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ExitFullscreen, weak_vr_shell_));
}

void VrGLThread::Navigate(GURL gurl, NavigationMethod method) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::Navigate, weak_vr_shell_, gurl, method));
}

void VrGLThread::NavigateBack() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::NavigateBack, weak_vr_shell_));
}

void VrGLThread::NavigateForward() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::NavigateForward, weak_vr_shell_));
}

void VrGLThread::ReloadTab() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ReloadTab, weak_vr_shell_));
}

void VrGLThread::OpenNewTab(bool incognito) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::OpenNewTab, weak_vr_shell_, incognito));
}

void VrGLThread::SelectTab(int id, bool incognito) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::SelectTab, weak_vr_shell_, id, incognito));
}

void VrGLThread::OpenBookmarks() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenBookmarks, weak_vr_shell_));
}

void VrGLThread::OpenRecentTabs() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenRecentTabs, weak_vr_shell_));
}

void VrGLThread::OpenHistory() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenHistory, weak_vr_shell_));
}

void VrGLThread::OpenDownloads() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenDownloads, weak_vr_shell_));
}

void VrGLThread::OpenShare() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenShare, weak_vr_shell_));
}

void VrGLThread::OpenSettings() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenSettings, weak_vr_shell_));
}

void VrGLThread::CloseTab(int id, bool incognito) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::CloseTab, weak_vr_shell_, id, incognito));
}

void VrGLThread::CloseAllTabs() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::CloseAllTabs, weak_vr_shell_));
}

void VrGLThread::CloseAllIncognitoTabs() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::CloseAllIncognitoTabs, weak_vr_shell_));
}

void VrGLThread::OpenFeedback() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OpenFeedback, weak_vr_shell_));
}

void VrGLThread::CloseHostedDialog() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::CloseHostedDialog, weak_vr_shell_));
}

void VrGLThread::ToggleCardboardGamepad(bool enabled) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ToggleCardboardGamepad,
                                weak_vr_shell_, enabled));
}

void VrGLThread::OnUnsupportedMode(UiUnsupportedMode mode) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::OnUnsupportedMode, weak_vr_shell_, mode));
}

void VrGLThread::OnExitVrPromptResult(ExitVrPromptChoice choice,
                                      UiUnsupportedMode reason) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OnExitVrPromptResult, weak_vr_shell_,
                                reason, choice));
}

void VrGLThread::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OnContentScreenBoundsChanged,
                                weak_vr_shell_, bounds));
}

void VrGLThread::SetVoiceSearchActive(bool active) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::SetVoiceSearchActive, weak_vr_shell_, active));
}

void VrGLThread::StartAutocomplete(const AutocompleteRequest& request) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::StartAutocomplete, weak_vr_shell_, request));
}

void VrGLThread::StopAutocomplete() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::StopAutocomplete, weak_vr_shell_));
}

void VrGLThread::ShowPageInfo() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ShowPageInfo, weak_vr_shell_));
}

void VrGLThread::SetFullscreen(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetFullscreen,
                                         weak_browser_ui_, enabled));
}

void VrGLThread::SetIncognito(bool incognito) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetIncognito,
                                         weak_browser_ui_, incognito));
}

void VrGLThread::SetHistoryButtonsEnabled(bool can_go_back,
                                          bool can_go_forward) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserUiInterface::SetHistoryButtonsEnabled,
                                weak_browser_ui_, can_go_back, can_go_forward));
}

void VrGLThread::SetLoadProgress(float progress) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetLoadProgress,
                                         weak_browser_ui_, progress));
}

void VrGLThread::SetLoading(bool loading) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetLoading,
                                         weak_browser_ui_, loading));
}

void VrGLThread::SetToolbarState(const ToolbarState& state) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetToolbarState,
                                         weak_browser_ui_, state));
}

void VrGLThread::SetWebVrMode(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::SetWebVrMode,
                                         weak_browser_ui_, enabled));
}

void VrGLThread::SetCapturingState(
    const CapturingStateModel& active_capturing,
    const CapturingStateModel& background_capturing,
    const CapturingStateModel& potential_capturing) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserUiInterface::SetCapturingState,
                                weak_browser_ui_, active_capturing,
                                background_capturing, potential_capturing));
}

void VrGLThread::ShowExitVrPrompt(UiUnsupportedMode reason) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::ShowExitVrPrompt,
                                         weak_browser_ui_, reason));
}

void VrGLThread::SetSpeechRecognitionEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::SetSpeechRecognitionEnabled,
                     weak_browser_ui_, enabled));
}

void VrGLThread::SetRecognitionResult(const base::string16& result) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserUiInterface::SetRecognitionResult,
                                weak_browser_ui_, result));
}

void VrGLThread::OnSpeechRecognitionStateChanged(int new_state) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::OnSpeechRecognitionStateChanged,
                     weak_browser_ui_, new_state));
}

void VrGLThread::SetOmniboxSuggestions(
    std::unique_ptr<OmniboxSuggestions> suggestions) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::SetOmniboxSuggestions,
                     weak_browser_ui_, base::Passed(std::move(suggestions))));
}

void VrGLThread::OnAssetsLoaded(AssetsLoadStatus status,
                                std::unique_ptr<Assets> assets,
                                const base::Version& component_version) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::OnAssetsLoaded, weak_browser_ui_,
                     status, std::move(assets), component_version));
}

void VrGLThread::OnAssetsUnavailable() {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserUiInterface::OnAssetsUnavailable,
                                weak_browser_ui_));
}

void VrGLThread::WaitForAssets() {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::WaitForAssets, weak_browser_ui_));
}

void VrGLThread::SetOverlayTextureEmpty(bool empty) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserUiInterface::SetOverlayTextureEmpty,
                                weak_browser_ui_, empty));
}

void VrGLThread::ShowSoftInput(bool show) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::ShowSoftInput,
                                         weak_browser_ui_, show));
}

void VrGLThread::UpdateWebInputIndices(int selection_start,
                                       int selection_end,
                                       int composition_start,
                                       int composition_end) {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindRepeating(&BrowserUiInterface::UpdateWebInputIndices,
                          weak_browser_ui_, selection_start, selection_end,
                          composition_start, composition_end));
}

void VrGLThread::OnSwapContents(int new_content_id) {
  task_runner()->PostTask(
      FROM_HERE, base::BindRepeating(&BrowserUiInterface::OnSwapContents,
                                     weak_browser_ui_, new_content_id));
}

void VrGLThread::SetDialogLocation(float x, float y) {
  task_runner()->PostTask(
      FROM_HERE, base::BindRepeating(&BrowserUiInterface::SetDialogLocation,
                                     weak_browser_ui_, x, y));
}

void VrGLThread::SetDialogFloating(bool floating) {
  task_runner()->PostTask(
      FROM_HERE, base::BindRepeating(&BrowserUiInterface::SetDialogFloating,
                                     weak_browser_ui_, floating));
}

void VrGLThread::ShowPlatformToast(const base::string16& text) {
  task_runner()->PostTask(
      FROM_HERE, base::BindRepeating(&BrowserUiInterface::ShowPlatformToast,
                                     weak_browser_ui_, text));
}

void VrGLThread::CancelPlatformToast() {
  task_runner()->PostTask(
      FROM_HERE, base::BindRepeating(&BrowserUiInterface::CancelPlatformToast,
                                     weak_browser_ui_));
}

void VrGLThread::OnContentBoundsChanged(int width, int height) {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindRepeating(&BrowserUiInterface::OnContentBoundsChanged,
                          weak_browser_ui_, width, height));
}

void VrGLThread::AddOrUpdateTab(int id,
                                bool incognito,
                                const base::string16& title) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserUiInterface::AddOrUpdateTab,
                                weak_browser_ui_, id, incognito, title));
}

void VrGLThread::RemoveTab(int id, bool incognito) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&BrowserUiInterface::RemoveTab,
                                         weak_browser_ui_, id, incognito));
}

void VrGLThread::RemoveAllTabs() {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::RemoveAllTabs, weak_browser_ui_));
}

void VrGLThread::PerformKeyboardInputForTesting(
    KeyboardTestInput keyboard_input) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserUiInterface::PerformKeyboardInputForTesting,
                     weak_browser_ui_, keyboard_input));
}

void VrGLThread::ReportUiOperationResultForTesting(
    const UiTestOperationType& action_type,
    const UiTestOperationResult& result) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ReportUiOperationResultForTesting,
                                weak_vr_shell_, action_type, result));
}

bool VrGLThread::OnMainThread() const {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

bool VrGLThread::OnGlThread() const {
  return task_runner()->BelongsToCurrentThread();
}

}  // namespace vr
