// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_GL_THREAD_H_
#define CHROME_BROWSER_ANDROID_VR_VR_GL_THREAD_H_

#include <memory>

#include "base/android/java_handler_thread.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/browser_renderer_factory.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/gvr_keyboard_delegate.h"
#include "chrome/browser/vr/browser_renderer_browser_interface.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/sound_id.h"
#include "chrome/browser/vr/platform_input_handler.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace base {
class Version;
class WaitableEvent;
}  // namespace base

namespace gvr {
class GvrApi;
}

namespace vr {

class VrInputConnection;
class VrShell;
struct KeyboardTestInput;

class VrGLThread : public base::android::JavaHandlerThread,
                   public PlatformInputHandler,
                   public BrowserRendererBrowserInterface,
                   public GlBrowserInterface,
                   public UiBrowserInterface,
                   public BrowserUiInterface {
 public:
  VrGLThread(
      const base::WeakPtr<VrShell>& weak_vr_shell,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gvr_context* gvr_api,
      const UiInitialState& ui_initial_state,
      bool reprojected_rendering,
      bool daydream_support,
      bool pause_content,
      bool low_density,
      base::WaitableEvent* gl_surface_created_event,
      base::OnceCallback<gfx::AcceleratedWidget()> surface_callback);

  ~VrGLThread() override;
  base::WeakPtr<BrowserRenderer> GetBrowserRenderer();
  void SetInputConnection(VrInputConnection* input_connection);

  // GlBrowserInterface implementation (GL calling to VrShell).
  void ContentSurfaceCreated(jobject surface,
                             gl::SurfaceTexture* texture) override;
  void ContentOverlaySurfaceCreated(jobject surface,
                                    gl::SurfaceTexture* texture) override;
  void GvrDelegateReady(gvr::ViewerType viewer_type) override;
  void SendRequestPresentReply(device::mojom::XRSessionPtr) override;
  void DialogSurfaceCreated(jobject surface,
                            gl::SurfaceTexture* texture) override;
  void UpdateGamepadData(device::GvrGamepadData) override;
  void ToggleCardboardGamepad(bool enabled) override;

  // BrowserRendererBrowserInterface implementation (BrowserRenderer calling to
  // VrShell).
  void ForceExitVr() override;
  void ReportUiOperationResultForTesting(
      const UiTestOperationType& action_type,
      const UiTestOperationResult& result) override;

  // PlatformInputHandler
  void ForwardEventToPlatformUi(std::unique_ptr<InputEvent> event) override;
  void ForwardEventToContent(std::unique_ptr<InputEvent> event,
                             int content_id) override;
  void ClearFocusedElement() override;
  void OnWebInputEdited(const TextEdits& edits) override;
  void SubmitWebInput() override;
  void RequestWebInputText(TextStateUpdateCallback callback) override;

  // UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;
  void ExitFullscreen() override;
  void Navigate(GURL gurl, NavigationMethod method) override;
  void NavigateBack() override;
  void NavigateForward() override;
  void ReloadTab() override;
  void OpenNewTab(bool incognito) override;
  void SelectTab(int id, bool incognito) override;
  void OpenBookmarks() override;
  void OpenRecentTabs() override;
  void OpenHistory() override;
  void OpenDownloads() override;
  void OpenShare() override;
  void OpenSettings() override;
  void CloseTab(int id, bool incognito) override;
  void CloseAllTabs() override;
  void CloseAllIncognitoTabs() override;
  void OpenFeedback() override;
  void CloseHostedDialog() override;
  void OnUnsupportedMode(UiUnsupportedMode mode) override;
  void OnExitVrPromptResult(ExitVrPromptChoice choice,
                            UiUnsupportedMode reason) override;
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) override;
  void SetVoiceSearchActive(bool active) override;
  void StartAutocomplete(const AutocompleteRequest& request) override;
  void StopAutocomplete() override;
  void ShowPageInfo() override;

  // BrowserUiInterface implementation (Browser calling to UI).
  void SetWebVrMode(bool enabled) override;
  void SetFullscreen(bool enabled) override;
  void SetToolbarState(const ToolbarState& state) override;
  void SetIncognito(bool incognito) override;
  void SetLoading(bool loading) override;
  void SetLoadProgress(float progress) override;
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) override;
  void SetCapturingState(
      const CapturingStateModel& active_capturing,
      const CapturingStateModel& background_capturing,
      const CapturingStateModel& potential_capturing) override;
  void ShowExitVrPrompt(UiUnsupportedMode reason) override;
  void SetSpeechRecognitionEnabled(bool enabled) override;
  void SetRecognitionResult(const base::string16& result) override;
  void OnSpeechRecognitionStateChanged(int new_state) override;
  void SetOmniboxSuggestions(
      std::unique_ptr<OmniboxSuggestions> result) override;
  void OnAssetsLoaded(AssetsLoadStatus status,
                      std::unique_ptr<Assets> assets,
                      const base::Version& component_version) override;
  void OnAssetsUnavailable() override;
  void WaitForAssets() override;
  void SetOverlayTextureEmpty(bool empty) override;
  void ShowSoftInput(bool show) override;
  void UpdateWebInputIndices(int selection_start,
                             int selection_end,
                             int composition_start,
                             int composition_end) override;
  void OnSwapContents(int new_content_id) override;
  void SetDialogLocation(float x, float y) override;
  void SetDialogFloating(bool floating) override;
  void ShowPlatformToast(const base::string16& text) override;
  void CancelPlatformToast() override;
  void OnContentBoundsChanged(int width, int height) override;
  void AddOrUpdateTab(int id,
                      bool incognito,
                      const base::string16& title) override;
  void RemoveTab(int id, bool incognito) override;
  void RemoveAllTabs() override;
  void PerformKeyboardInputForTesting(
      KeyboardTestInput keyboard_input) override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  bool OnMainThread() const;
  bool OnGlThread() const;

  base::WeakPtr<VrShell> weak_vr_shell_;
  base::WeakPtr<BrowserUiInterface> weak_browser_ui_;
  base::WeakPtr<VrInputConnection> weak_input_connection_;
  // Both VrInputConnection and VrGlThread are owned by VrShell. In VrShell, we
  // made sure that this input_connection_ is up to date and destroyed after
  // VrGlThread. So it is safe to use raw pointer here.
  VrInputConnection* input_connection_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Created on GL thread.
  std::unique_ptr<UiFactory> ui_factory_;
  std::unique_ptr<BrowserRenderer> browser_renderer_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;

  // This state is used for initializing the BrowserRenderer.
  std::unique_ptr<BrowserRendererFactory::Params> factory_params_;

  DISALLOW_COPY_AND_ASSIGN(VrGLThread);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_GL_THREAD_H_
