// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/exit_vr_prompt_choice.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_initial_state.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/page_info/page_info_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace base {
class Version;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace gl {
class SurfaceTexture;
}

namespace vr {

class BrowserUiInterface;
class AndroidUiGestureTarget;
class AutocompleteController;
class LocationBarHelper;
class VrGLThread;
class VrInputConnection;
class VrShellDelegate;
class VrWebContentsObserver;
enum class UiTestOperationType;
enum class UiTestOperationResult;
struct Assets;
struct AutocompleteRequest;

// The native instance of the Java VrShell. This class is not threadsafe and
// must only be used on the UI thread.
class VrShell : VoiceResultDelegate,
                public ChromeLocationBarModelDelegate,
                public PageInfoUI {
 public:
  VrShell(JNIEnv* env,
          const base::android::JavaParamRef<jobject>& obj,
          const UiInitialState& ui_initial_state,
          VrShellDelegate* delegate,
          gvr_context* gvr_api,
          bool reprojected_rendering,
          float display_width_meters,
          float display_height_meters,
          int display_width_pixels,
          int display_height_pixels,
          bool pause_content,
          bool low_density);

  VrShell(const VrShell&) = delete;
  VrShell& operator=(const VrShell&) = delete;

  bool HasUiFinishedLoading(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  void SwapContents(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jobject>& web_contents);
  void SetAndroidGestureTarget(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& android_ui_gesture_target);
  void SetDialogGestureTarget(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& dialog_gesture_target);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      bool touched);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetSurface(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& surface);
  void SetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    bool enabled);
  bool GetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  bool IsDisplayingUrlForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetVrInputConnectionForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void OnLoadProgressChanged(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             double progress);
  void OnTabListCreated(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jobjectArray>& tabs,
      const base::android::JavaRef<jobjectArray>& incognito_tabs);
  void OnTabUpdated(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jboolean incognito,
                    jint id,
                    jstring jtitle);
  void OnTabRemoved(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jboolean incognito,
                    jint id);
  void Navigate(GURL url, NavigationMethod method);
  void NavigateBack();
  void NavigateForward();
  void ReloadTab();
  void OpenNewTab(bool incognito);
  void OpenBookmarks();
  void OpenRecentTabs();
  void OpenHistory();
  void OpenDownloads();
  void OpenShare();
  void OpenSettings();
  void CloseAllIncognitoTabs();
  void OpenFeedback();
  void CloseHostedDialog();
  void ToggleCardboardGamepad(bool enabled);
  void SetHistoryButtonsEnabled(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jboolean can_go_back,
                                jboolean can_go_forward);
  void RequestToExitVr(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       int reason);

  void ShowSoftInput(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     bool show);
  void UpdateWebInputIndices(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int selection_start,
                             int selection_end,
                             int composition_start,
                             int composition_end);
  void ResumeContentRendering(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void OnOverlayTextureEmptyChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean empty);

  void ContentWebContentsDestroyed();

  void ContentSurfaceCreated(jobject surface, gl::SurfaceTexture* texture);
  void ContentOverlaySurfaceCreated(jobject surface,
                                    gl::SurfaceTexture* texture);
  void GvrDelegateReady();
  void SendRequestPresentReply(device::mojom::XRSessionPtr);

  void DialogSurfaceCreated(jobject surface, gl::SurfaceTexture* texture);

  void BufferBoundsChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& object,
                           jint content_width,
                           jint content_height,
                           jint overlay_width,
                           jint overlay_height);

  void ForceExitVr();
  void ExitPresent();
  void ExitFullscreen();
  void OnUnsupportedMode(UiUnsupportedMode mode);
  void OnExitVrPromptResult(UiUnsupportedMode reason,
                            ExitVrPromptChoice choice);
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds);
  void SetVoiceSearchActive(bool active);
  void StartAutocomplete(const AutocompleteRequest& request);
  void StopAutocomplete();
  void ShowPageInfo();
  bool HasRecordAudioPermission() const;
  bool CanRequestRecordAudioPermission() const;
  void RequestRecordAudioPermissionResult(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jboolean can_record_audio);

  void ClearFocusedElement();
  void ProcessContentGesture(std::unique_ptr<InputEvent> event, int content_id);

  void ProcessDialogGesture(std::unique_ptr<InputEvent> event);

  void SetAlertDialog(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      float width,
                      float height);
  void CloseAlertDialog(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void SetDialogBufferSize(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           int width,
                           int height);
  void SetAlertDialogSize(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          float width,
                          float height);
  void SetDialogLocation(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         float x,
                         float y);
  void SetDialogFloating(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         bool floating);

  void ShowToast(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jstring text);
  void CancelToast(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options);

  // ChromeLocationBarModelDelegate implementation.
  content::WebContents* GetActiveWebContents() const override;
  bool ShouldDisplayURL() const override;

  void OnVoiceResults(const std::u16string& result) override;

  void OnAssetsLoaded(AssetsLoadStatus status,
                      std::unique_ptr<Assets> assets,
                      const base::Version& component_version);

  // PageInfoUI implementation.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetPageFeatureInfo(const PageFeatureInfo& info) override;

  void AcceptDoffPromptForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void SetUiExpectingActivityForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint quiescence_timeout_ms);

  void SaveNextFrameBufferToDiskForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jstring filepath_base);

  void WatchElementForVisibilityStatusForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint element_name,
      jint timeout_ms,
      jboolean visibility);

  void ReportUiOperationResultForTesting(UiTestOperationType action_type,
                                         UiTestOperationResult result);

  void PerformControllerActionForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint element_name,
      jint action_type,
      jfloat x,
      jfloat y);

  void PerformKeyboardInputForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint input_type,
      jstring input_string);
  gfx::AcceleratedWidget GetRenderSurface();

 private:
  ~VrShell() override;
  void PostToGlThread(const base::Location& from_here, base::OnceClosure task);
  void SetUiState();

  void PollCapturingState();

  bool HasDaydreamSupport(JNIEnv* env);

  content::WebContents* GetNonNativePageWebContents() const;

  void LoadAssets();
  void OnAssetsComponentReady();
  void OnAssetsComponentWaitTimeout();

  void SetWebContents(content::WebContents* contents);
  std::unique_ptr<PageInfo> CreatePageInfo();

  bool webvr_mode_ = false;

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  bool web_contents_is_native_page_ = false;
  base::android::ScopedJavaGlobalRef<jobject> j_motion_event_synthesizer_;

  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;
  // Note this must be destroyed after VrGLThread is destroyed in the
  // destruction of VrShell. VrGLThread keeps a raw pointer of VrInputConnection
  // and uses the pointer on GL thread.
  std::unique_ptr<VrInputConnection> vr_input_connection_;

  raw_ptr<VrShellDelegate> delegate_provider_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  std::unique_ptr<AndroidUiGestureTarget> android_ui_gesture_target_;
  std::unique_ptr<AndroidUiGestureTarget> dialog_gesture_target_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<VrGLThread> gl_thread_;
  raw_ptr<BrowserUiInterface> ui_;

  // These instances make use of ui_ (provided by gl_thread_), and hence must be
  // destroyed before gl_thread_;
  std::unique_ptr<LocationBarHelper> toolbar_;
  std::unique_ptr<vr::AutocompleteController> autocomplete_controller_;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;

  bool reprojected_rendering_;

  mojo::Remote<device::mojom::GeolocationConfig> geolocation_config_;

  base::CancelableOnceClosure poll_capturing_state_task_;
  CapturingStateModel active_capturing_;
  CapturingStateModel background_capturing_;
  CapturingStateModel potential_capturing_;

  // Are we currently providing a gamepad factory to the gamepad manager?
  bool cardboard_gamepad_source_active_ = false;
  bool pending_cardboard_trigger_ = false;

  int64_t cardboard_gamepad_timer_ = 0;

  // Content id
  int content_id_ = 0;

  gfx::SizeF display_size_meters_;
  gfx::Size display_size_pixels_;

  raw_ptr<gl::SurfaceTexture> content_surface_texture_ = nullptr;
  raw_ptr<gl::SurfaceTexture> overlay_surface_texture_ = nullptr;
  raw_ptr<gl::SurfaceTexture> ui_surface_texture_ = nullptr;

  base::OneShotTimer waiting_for_assets_component_timer_;
  bool can_load_new_assets_ = false;
  bool ui_finished_loading_ = false;

  base::WaitableEvent gl_surface_created_event_;
  gfx::AcceleratedWidget surface_window_ = nullptr;

  std::set<int> regular_tab_ids_;
  std::set<int> incognito_tab_ids_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_
