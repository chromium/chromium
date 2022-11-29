// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell.h"

#include <android/native_window_jni.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/android/features/vr/split_jni_headers/VrShell_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/android_ui_gesture_target.h"
#include "chrome/browser/android/vr/autocomplete_controller.h"
#include "chrome/browser/android/vr/vr_gl_thread.h"
#include "chrome/browser/android/vr/vr_input_connection.h"
#include "chrome/browser/android/vr/vr_shell_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/location_bar_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/vr_web_contents_observer.h"
#include "chrome/common/url_constants.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/android/surface_texture.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {
vr::VrShell* g_vr_shell_instance;

constexpr base::TimeDelta kPollCapturingStateInterval = base::Seconds(0.2);

constexpr base::TimeDelta kAssetsComponentWaitDelay = base::Seconds(2);

static constexpr float kInchesToMeters = 0.0254f;
// Screen pixel density of the Google Pixel phone in pixels per inch.
static constexpr float kPixelPpi = 441.0f;
// Screen pixel density of the Google Pixel phone in pixels per meter.
static constexpr float kPixelPpm = kPixelPpi / kInchesToMeters;

// Factor to adjust the legibility of the content. Making this factor smaller
// increases the text size.
static constexpr float kContentLegibilityFactor = 1.36f;

// This factor converts the physical width of the projected
// content quad into the required width of the virtual content window.
// TODO(tiborg): This value is calibrated for the Google Pixel. We should adjust
// this value dynamically based on the target device's pixel density in the
// future.
static constexpr float kContentBoundsMetersToWindowSize =
    kPixelPpm * kContentLegibilityFactor;

// Factor by which the content's pixel amount is increased beyond what the
// projected content quad covers in screen real estate.
// This DPR factor works well on Pixel phones.
static constexpr float kContentDprFactor = 4.0f;

void SetIsInVR(content::WebContents* contents, bool is_in_vr) {
  if (contents && contents->GetRenderWidgetHostView()) {
    // TODO(asimjour) Contents should not be aware of VR mode. Instead, we
    // should add a flag for disabling specific UI such as the keyboard (see
    // VrTabHelper for details).
    contents->GetRenderWidgetHostView()->SetIsInVR(is_in_vr);

    VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
    DCHECK(vr_tab_helper);
    vr_tab_helper->SetIsInVr(is_in_vr);
  }
}

}  // namespace

VrShell::VrShell(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const UiInitialState& ui_initial_state,
                 VrShellDelegate* delegate,
                 gvr_context* gvr_api,
                 bool reprojected_rendering,
                 float display_width_meters,
                 float display_height_meters,
                 int display_width_pixels,
                 int display_height_pixels,
                 bool pause_content,
                 bool low_density)
    : delegate_provider_(delegate),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      reprojected_rendering_(reprojected_rendering),
      display_size_meters_(display_width_meters, display_height_meters),
      display_size_pixels_(display_width_pixels, display_height_pixels),
      gl_surface_created_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  DCHECK(g_vr_shell_instance == nullptr);
  g_vr_shell_instance = this;
  j_vr_shell_.Reset(env, obj);

  base::OnceCallback<gfx::AcceleratedWidget()> surface_callback =
      base::BindOnce(&VrShell::GetRenderSurface, base::Unretained(this));

  gl_thread_ = std::make_unique<VrGLThread>(
      weak_ptr_factory_.GetWeakPtr(), main_thread_task_runner_, gvr_api,
      ui_initial_state, reprojected_rendering_, HasDaydreamSupport(env),
      pause_content, low_density, &gl_surface_created_event_,
      std::move(surface_callback));
  ui_ = gl_thread_.get();
  toolbar_ = std::make_unique<LocationBarHelper>(ui_, this);
  autocomplete_controller_ =
      std::make_unique<AutocompleteController>(base::BindRepeating(
          &BrowserUiInterface::SetOmniboxSuggestions, base::Unretained(ui_)));

  gl_thread_->Start();

  can_load_new_assets_ = AssetsLoader::GetInstance()->ComponentReady();
  if (!can_load_new_assets_) {
    waiting_for_assets_component_timer_.Start(
        FROM_HERE, kAssetsComponentWaitDelay,
        base::BindOnce(&VrShell::OnAssetsComponentWaitTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  AssetsLoader::GetInstance()->SetOnComponentReadyCallback(base::BindRepeating(
      &VrShell::OnAssetsComponentReady, weak_ptr_factory_.GetWeakPtr()));

  UpdateVrAssetsComponent(g_browser_process->component_updater());

  content::GetDeviceService().BindGeolocationConfig(
      geolocation_config_.BindNewPipeAndPassReceiver());
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

bool VrShell::HasUiFinishedLoading(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return ui_finished_loading_;
}

void VrShell::SwapContents(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           const JavaParamRef<jobject>& tab) {
  content_id_++;
  gl_thread_->OnSwapContents(content_id_);
  TabAndroid* active_tab =
      tab.is_null()
          ? nullptr
          : TabAndroid::GetNativeTab(env, JavaParamRef<jobject>(env, tab));

  content::WebContents* contents =
      active_tab ? active_tab->web_contents() : nullptr;
  bool is_native_page = active_tab ? active_tab->IsNativePage() : true;

  SetIsInVR(GetNonNativePageWebContents(), false);

  web_contents_ = contents;
  web_contents_is_native_page_ = is_native_page;
  SetIsInVR(GetNonNativePageWebContents(), true);
  SetUiState();

  if (web_contents_) {
    vr_input_connection_.reset(new VrInputConnection(web_contents_));
    PostToGlThread(FROM_HERE, base::BindOnce(&VrGLThread::SetInputConnection,
                                             base::Unretained(gl_thread_.get()),
                                             vr_input_connection_.get()));
  } else {
    vr_input_connection_ = nullptr;
    PostToGlThread(FROM_HERE,
                   base::BindOnce(&VrGLThread::SetInputConnection,
                                  base::Unretained(gl_thread_.get()), nullptr));
  }

  vr_web_contents_observer_ = std::make_unique<VrWebContentsObserver>(
      web_contents_, ui_, toolbar_.get(),
      base::BindOnce(&VrShell::ContentWebContentsDestroyed,
                     base::Unretained(this)));
}

void VrShell::SetAndroidGestureTarget(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& android_ui_gesture_target) {
  android_ui_gesture_target_.reset(
      AndroidUiGestureTarget::FromJavaObject(android_ui_gesture_target));
}

void VrShell::SetDialogGestureTarget(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& dialog_gesture_target) {
  dialog_gesture_target_.reset(
      AndroidUiGestureTarget::FromJavaObject(dialog_gesture_target));
}

void VrShell::SetUiState() {
  toolbar_->Update();

  if (!GetNonNativePageWebContents()) {
    ui_->SetLoading(false);
    ui_->SetFullscreen(false);
  } else {
    ui_->SetLoading(GetNonNativePageWebContents()->IsLoading());
    ui_->SetFullscreen(GetNonNativePageWebContents()->IsFullscreen());
  }
  if (web_contents_) {
    ui_->SetIncognito(web_contents_->GetBrowserContext()->IsOffTheRecord());
  } else {
    ui_->SetIncognito(false);
  }
}

VrShell::~VrShell() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  content_surface_texture_ = nullptr;
  overlay_surface_texture_ = nullptr;

  delegate_provider_->RemoveDelegate();
  {
    // The GvrLayout is, and must always be, used only on the UI thread, and the
    // GvrApi used for rendering should only be used from the GL thread as it's
    // not thread safe. However, the GvrLayout owns the GvrApi instance, and
    // when it gets shut down it deletes the GvrApi instance with it. Therefore,
    // we need to block shutting down the GvrLayout on stopping our GL thread
    // from using the GvrApi instance.
    // base::Thread::Stop, which is called when destroying the thread, asserts
    // that sync primitives are allowed to prevent jank, but there shouldn't be
    // any concerns regarding jank in this case, because we're switching from 3D
    // to 2D, adding/removing a bunch of Java views, and probably changing
    // device orientation here.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
    gl_thread_.reset();
  }
  g_vr_shell_instance = nullptr;
}

void VrShell::PostToGlThread(const base::Location& from_here,
                             base::OnceClosure task) {
  gl_thread_->task_runner()->PostTask(from_here, std::move(task));
}

void VrShell::Navigate(GURL url, NavigationMethod method) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_VrShell_loadUrl(env, j_vr_shell_,
                       base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

void VrShell::NavigateBack() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_navigateBack(env, j_vr_shell_);
}

void VrShell::NavigateForward() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_navigateForward(env, j_vr_shell_);
}

void VrShell::ReloadTab() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_reloadTab(env, j_vr_shell_);
}

void VrShell::OpenNewTab(bool incognito) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openNewTab(env, j_vr_shell_, incognito);
}

void VrShell::OpenBookmarks() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openBookmarks(env, j_vr_shell_);
}

void VrShell::OpenRecentTabs() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openRecentTabs(env, j_vr_shell_);
}

void VrShell::OpenHistory() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openHistory(env, j_vr_shell_);
}

void VrShell::OpenDownloads() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openDownloads(env, j_vr_shell_);
}

void VrShell::OpenShare() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openShare(env, j_vr_shell_);
}

void VrShell::OpenSettings() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openSettings(env, j_vr_shell_);
}

void VrShell::CloseAllIncognitoTabs() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_closeAllIncognitoTabs(env, j_vr_shell_);
}

void VrShell::OpenFeedback() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_openFeedback(env, j_vr_shell_);
}

void VrShell::CloseHostedDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_closeCurrentDialog(env, j_vr_shell_);
}

void VrShell::ToggleCardboardGamepad(bool enabled) {
  // Enable/disable updating gamepad state.
  if (cardboard_gamepad_source_active_ && !enabled) {
    cardboard_gamepad_source_active_ = false;
  }

  if (!cardboard_gamepad_source_active_ && enabled) {
    device::GvrDevice* gvr_device = delegate_provider_->GetGvrDevice();
    if (!gvr_device)
      return;
    cardboard_gamepad_source_active_ = true;
    if (pending_cardboard_trigger_) {
      OnTriggerEvent(nullptr, JavaParamRef<jobject>(nullptr), true);
    }
    pending_cardboard_trigger_ = false;
  }
}

void VrShell::OnTriggerEvent(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             bool touched) {
  // If we are running cardboard, update gamepad state.
  if (cardboard_gamepad_source_active_) {
    cardboard_gamepad_timer_++;
  } else {
    pending_cardboard_trigger_ = touched;
  }

  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::OnTriggerEvent,
                                gl_thread_->GetBrowserRenderer(), touched));
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::OnPause,
                                           gl_thread_->GetBrowserRenderer()));

  poll_capturing_state_task_.Cancel();
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  // Calling WaitForAssets before BrowserRenderer::OnResume so that the UI won't
  // accidentally produce an initial frame with UI.
  if (can_load_new_assets_) {
    ui_->WaitForAssets();
    LoadAssets();
  }

  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::OnResume,
                                           gl_thread_->GetBrowserRenderer()));

  PollCapturingState();
}

void VrShell::SetSurface(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& surface) {
  DCHECK(!reprojected_rendering_);
  DCHECK(!surface.is_null());
  gfx::AcceleratedWidget window =
      ANativeWindow_fromSurface(base::android::AttachCurrentThread(), surface);
  surface_window_ = window;
  gl_surface_created_event_.Signal();
}

void VrShell::SetWebVrMode(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           bool enabled) {
  webvr_mode_ = enabled;
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::SetWebXrMode,
                                gl_thread_->GetBrowserRenderer(), enabled));
  // We create and dispose a page info in order to get notifed of page
  // permissions.
  CreatePageInfo();
  ui_->SetWebVrMode(enabled);
}

bool VrShell::GetWebVrMode(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return webvr_mode_;
}

bool VrShell::IsDisplayingUrlForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return ShouldDisplayURL();
}

base::android::ScopedJavaLocalRef<jobject>
VrShell::GetVrInputConnectionForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!vr_input_connection_)
    return nullptr;

  return vr_input_connection_->GetJavaObject();
}

void VrShell::OnLoadProgressChanged(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    double progress) {
  ui_->SetLoadProgress(progress);
}

void VrShell::OnTabListCreated(JNIEnv* env,
                               const JavaRef<jobject>& obj,
                               const JavaRef<jobjectArray>& tabs,
                               const JavaRef<jobjectArray>& incognito_tabs) {
  incognito_tab_ids_.clear();
  regular_tab_ids_.clear();
  for (auto j_tab : incognito_tabs.ReadElements<jobject>()) {
    TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
    incognito_tab_ids_.insert(tab->GetAndroidId());
  }

  for (auto j_tab : tabs.ReadElements<jobject>()) {
    TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
    regular_tab_ids_.insert(tab->GetAndroidId());
  }
  ui_->SetIncognitoTabsOpen(!incognito_tab_ids_.empty());
  ui_->SetRegularTabsOpen(!regular_tab_ids_.empty());
}

void VrShell::OnTabUpdated(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id,
                           jstring jtitle) {
  if (incognito) {
    incognito_tab_ids_.insert(id);
    ui_->SetIncognitoTabsOpen(!incognito_tab_ids_.empty());
  } else {
    regular_tab_ids_.insert(id);
    ui_->SetRegularTabsOpen(!regular_tab_ids_.empty());
  }
}

void VrShell::OnTabRemoved(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id) {
  if (incognito) {
    incognito_tab_ids_.erase(id);
    ui_->SetIncognitoTabsOpen(!incognito_tab_ids_.empty());
  } else {
    regular_tab_ids_.erase(id);
    ui_->SetRegularTabsOpen(!regular_tab_ids_.empty());
  }
}

void VrShell::SetAlertDialog(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             float width,
                             float height) {
  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::EnableAlertDialog,
                                           gl_thread_->GetBrowserRenderer(),
                                           gl_thread_.get(), width, height));
}

void VrShell::CloseAlertDialog(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::DisableAlertDialog,
                                           gl_thread_->GetBrowserRenderer()));
  // This will refresh our permissions after an alert is closed which should
  // ensure that long press on the app button gives accurate results.
  CreatePageInfo();
}

void VrShell::SetDialogBufferSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int width,
    int height) {
  if (ui_surface_texture_)
    ui_surface_texture_->SetDefaultBufferSize(width, height);
}

void VrShell::SetAlertDialogSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    float width,
    float height) {
  PostToGlThread(FROM_HERE, base::BindOnce(&BrowserRenderer::SetAlertDialogSize,
                                           gl_thread_->GetBrowserRenderer(),
                                           width, height));
}

void VrShell::SetDialogLocation(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                float x,
                                float y) {
  gl_thread_->SetDialogLocation(x, y);
}

void VrShell::SetDialogFloating(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                bool floating) {
  gl_thread_->SetDialogFloating(floating);
}

void VrShell::ShowToast(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jstring jtext) {
  std::u16string text;
  base::android::ConvertJavaStringToUTF16(env, jtext, &text);
  gl_thread_->ShowPlatformToast(text);
}

void VrShell::CancelToast(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj) {
  gl_thread_->CancelPlatformToast();
}

void VrShell::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(&BrowserRenderer::ConnectPresentingService,
                     gl_thread_->GetBrowserRenderer(), std::move(options)));
}

void VrShell::SetHistoryButtonsEnabled(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean can_go_back,
                                       jboolean can_go_forward) {
  ui_->SetHistoryButtonsEnabled(can_go_back, can_go_forward);
}

void VrShell::RequestToExitVr(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              int reason) {
  ui_->ShowExitVrPrompt(static_cast<UiUnsupportedMode>(reason));
}

void VrShell::ContentSurfaceCreated(jobject surface,
                                    gl::SurfaceTexture* texture) {
  content_surface_texture_ = texture;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> ref(env, surface);
  Java_VrShell_contentSurfaceCreated(env, j_vr_shell_, ref);
}

void VrShell::ContentOverlaySurfaceCreated(jobject surface,
                                           gl::SurfaceTexture* texture) {
  overlay_surface_texture_ = texture;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> ref(env, surface);
  Java_VrShell_contentOverlaySurfaceCreated(env, j_vr_shell_, ref);
}

void VrShell::DialogSurfaceCreated(jobject surface,
                                   gl::SurfaceTexture* texture) {
  ui_surface_texture_ = texture;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> ref(env, surface);
  Java_VrShell_dialogSurfaceCreated(env, j_vr_shell_, ref);
}

void VrShell::GvrDelegateReady() {
  delegate_provider_->SetDelegate(this);
}

void VrShell::SendRequestPresentReply(device::mojom::XRSessionPtr session) {
  delegate_provider_->SendRequestPresentReply(std::move(session));
}

void VrShell::BufferBoundsChanged(JNIEnv* env,
                                  const JavaParamRef<jobject>& object,
                                  jint content_width,
                                  jint content_height,
                                  jint overlay_width,
                                  jint overlay_height) {
  TRACE_EVENT0("gpu", "VrShell::ContentPhysicalBoundsChanged");
  if (content_surface_texture_) {
    content_surface_texture_->SetDefaultBufferSize(content_width,
                                                   content_height);
  }
  if (overlay_surface_texture_) {
    overlay_surface_texture_->SetDefaultBufferSize(overlay_width,
                                                   overlay_height);
  }
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::BufferBoundsChanged,
                                gl_thread_->GetBrowserRenderer(),
                                gfx::Size(content_width, content_height),
                                gfx::Size(overlay_width, overlay_height)));
}

void VrShell::ResumeContentRendering(JNIEnv* env,
                                     const JavaParamRef<jobject>& object) {
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::ResumeContentRendering,
                                gl_thread_->GetBrowserRenderer()));
}

void VrShell::OnOverlayTextureEmptyChanged(JNIEnv* env,
                                           const JavaParamRef<jobject>& object,
                                           jboolean empty) {
  ui_->SetOverlayTextureEmpty(empty);
}

void VrShell::ContentWebContentsDestroyed() {
  web_contents_ = nullptr;
}

void VrShell::ForceExitVr() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_forceExitVr(env, j_vr_shell_);
}

void VrShell::ExitPresent() {
  delegate_provider_->ExitWebVRPresent();
}

void VrShell::ExitFullscreen() {
  if (GetNonNativePageWebContents() &&
      GetNonNativePageWebContents()->IsFullscreen()) {
    GetNonNativePageWebContents()->ExitFullscreen(false);
  }
}

void VrShell::ShowSoftInput(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            bool show) {
  ui_->ShowSoftInput(show);
}

void VrShell::UpdateWebInputIndices(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int selection_start,
    int selection_end,
    int composition_start,
    int composition_end) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrGLThread::UpdateWebInputIndices,
                                           base::Unretained(gl_thread_.get()),
                                           selection_start, selection_end,
                                           composition_start, composition_end));
}

content::WebContents* VrShell::GetNonNativePageWebContents() const {
  return !web_contents_is_native_page_ ? web_contents_.get() : nullptr;
}

void VrShell::OnUnsupportedMode(UiUnsupportedMode mode) {
  switch (mode) {
    case UiUnsupportedMode::kUnhandledCodePoint:
      // We should never have this case.
      CHECK(false);
      return;
    case UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission: {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_VrShell_onUnhandledPermissionPrompt(env, j_vr_shell_);
      return;
    }
    case UiUnsupportedMode::kNeedsKeyboardUpdate: {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_VrShell_onNeedsKeyboardUpdate(env, j_vr_shell_);
      return;
    }
    // These modes are not sent by the UI anymore. Enum values still exist to
    // show correct exit prompt if vr-browsing-native-android-ui flag is false.
    case UiUnsupportedMode::kUnhandledPageInfo:
    case UiUnsupportedMode::kUnhandledCertificateInfo:
    case UiUnsupportedMode::kUnhandledConnectionSecurityInfo:
    case UiUnsupportedMode::kGenericUnsupportedFeature:
    // kSearchEnginePromo should directly DOFF without showing a promo. So it
    // should never be used from VR ui thread.
    case UiUnsupportedMode::kSearchEnginePromo:
    case UiUnsupportedMode::kCount:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

void VrShell::OnExitVrPromptResult(UiUnsupportedMode reason,
                                   ExitVrPromptChoice choice) {
  bool should_exit;
  switch (choice) {
    case ExitVrPromptChoice::CHOICE_NONE:
    case ExitVrPromptChoice::CHOICE_STAY:
      should_exit = false;
      break;
    case ExitVrPromptChoice::CHOICE_EXIT:
      should_exit = true;
      break;
  }

  DCHECK_NE(reason, UiUnsupportedMode::kCount);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_onExitVrRequestResult(env, j_vr_shell_, should_exit);
}

void VrShell::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {
  // We have to fit the content into the portion of the display for one eye.
  // Thus, take only half the width.
  int width_pixels = display_size_pixels_.width() / 2;
  float width_meters = display_size_meters_.width() / 2;

  // The physical and pixel dimensions to draw the scene for one eye needs to be
  // a square so that the content's aspect ratio is preserved (i.e. make pixels
  // square for the calculations).

  // For the resolution err on the side of too many pixels so that our content
  // is rather drawn with too high of a resolution than too low.
  int length_pixels = std::max(width_pixels, display_size_pixels_.height());

  // For the size err on the side of a too small area so that the font size is
  // rather too big than too small.
  float length_meters = std::min(width_meters, display_size_meters_.height());

  // Calculate the virtual window size and DPR and pass this to VrShell.
  gfx::Size window_size = gfx::ToRoundedSize(
      gfx::ScaleSize(bounds, width_meters * kContentBoundsMetersToWindowSize));
  // Need to use sqrt(kContentDprFactor) to translate from a factor applicable
  // to the area to a factor applicable to one side length.
  float dpr =
      (length_pixels / (length_meters * kContentBoundsMetersToWindowSize)) *
      std::sqrt(kContentDprFactor);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_setContentCssSize(env, j_vr_shell_, window_size.width(),
                                 window_size.height(), dpr);

  gl_thread_->OnContentBoundsChanged(window_size.width(), window_size.height());
}

void VrShell::SetVoiceSearchActive(bool active) {
  if (!active && !speech_recognizer_)
    return;

  if (!HasRecordAudioPermission()) {
    OnUnsupportedMode(
        UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
    return;
  }

  if (!speech_recognizer_) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    std::string profile_locale = g_browser_process->GetApplicationLocale();
    speech_recognizer_.reset(new SpeechRecognizer(
        this, ui_,
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
        profile_locale));
  }
  if (active) {
    speech_recognizer_->Start();
  } else {
    speech_recognizer_->Stop();
  }
}

void VrShell::StartAutocomplete(const AutocompleteRequest& request) {
  autocomplete_controller_->Start(request);
}

void VrShell::StopAutocomplete() {
  autocomplete_controller_->Stop();
}

void VrShell::ShowPageInfo() {
  Java_VrShell_showPageInfo(base::android::AttachCurrentThread(), j_vr_shell_);
}

bool VrShell::HasRecordAudioPermission() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_VrShell_hasRecordAudioPermission(env, j_vr_shell_);
}

bool VrShell::CanRequestRecordAudioPermission() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_VrShell_canRequestRecordAudioPermission(env, j_vr_shell_);
}

void VrShell::RequestRecordAudioPermissionResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& object,
    jboolean can_record_audio) {
  // If permission was denied, we need to check if it was *permanently* denied.
  if (!can_record_audio && !CanRequestRecordAudioPermission()) {
    ui_->SetHasOrCanRequestRecordAudioPermission(false);
  }
}

void VrShell::PollCapturingState() {
  poll_capturing_state_task_.Reset(
      base::BindOnce(&VrShell::PollCapturingState, base::Unretained(this)));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE, poll_capturing_state_task_.callback(),
      kPollCapturingStateInterval);

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  active_capturing_.audio_capture_enabled = false;
  active_capturing_.video_capture_enabled = false;
  active_capturing_.screen_capture_enabled = false;
  active_capturing_.bluetooth_connected = false;
  background_capturing_.audio_capture_enabled = false;
  background_capturing_.video_capture_enabled = false;
  background_capturing_.screen_capture_enabled = false;
  background_capturing_.bluetooth_connected = false;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* rwh = widgets->GetNextHost()) {
    bool is_foreground = rwh->GetProcess()->VisibleClientCount() > 0;
    content::RenderViewHost* rvh = content::RenderViewHost::From(rwh);
    if (!rvh)
      continue;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      continue;
    if (web_contents->GetRenderViewHost() != rvh)
      continue;

    // Because a WebContents can only have one current RVH at a time, there will
    // be no duplicate WebContents here.
    if (indicator->IsCapturingAudio(web_contents)) {
      if (is_foreground)
        active_capturing_.audio_capture_enabled = true;
      else
        background_capturing_.audio_capture_enabled = true;
    }
    if (indicator->IsCapturingVideo(web_contents)) {
      if (is_foreground)
        active_capturing_.video_capture_enabled = true;
      else
        background_capturing_.video_capture_enabled = true;
    }
    if (indicator->IsBeingMirrored(web_contents)) {
      if (is_foreground)
        active_capturing_.screen_capture_enabled = true;
      else
        background_capturing_.screen_capture_enabled = true;
    }
    if (web_contents->IsConnectedToBluetoothDevice()) {
      if (is_foreground)
        active_capturing_.bluetooth_connected = true;
      else
        background_capturing_.bluetooth_connected = true;
    }
  }

  geolocation_config_->IsHighAccuracyLocationBeingCaptured(base::BindOnce(
      [](VrShell* shell, BrowserUiInterface* ui,
         CapturingStateModel* active_capturing,
         CapturingStateModel* background_capturing,
         CapturingStateModel* potential_capturing,
         bool high_accuracy_location) {
        active_capturing->location_access_enabled = high_accuracy_location;
        ui->SetCapturingState(*active_capturing, *background_capturing,
                              *potential_capturing);
      },
      base::Unretained(this), base::Unretained(ui_),
      base::Unretained(&active_capturing_),
      base::Unretained(&background_capturing_),
      base::Unretained(&potential_capturing_)));
}

void VrShell::ClearFocusedElement() {
  if (!web_contents_)
    return;

  web_contents_->ClearFocusedElement();
}

void VrShell::ProcessContentGesture(std::unique_ptr<InputEvent> event,
                                    int content_id) {
  // Block the events if they don't belong to the current content
  if (content_id_ != content_id)
    return;

  if (!android_ui_gesture_target_)
    return;

  android_ui_gesture_target_->DispatchInputEvent(std::move(event));
}

void VrShell::ProcessDialogGesture(std::unique_ptr<InputEvent> event) {
  if (!dialog_gesture_target_)
    return;

  dialog_gesture_target_->DispatchInputEvent(std::move(event));
}

bool VrShell::HasDaydreamSupport(JNIEnv* env) {
  return Java_VrShell_hasDaydreamSupport(env, j_vr_shell_);
}

content::WebContents* VrShell::GetActiveWebContents() const {
  // TODO(tiborg): Handle the case when Tab#isShowingErrorPage returns true.
  return web_contents_;
}

bool VrShell::ShouldDisplayURL() const {
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry || entry->IsInitialEntry()) {
    return ChromeLocationBarModelDelegate::ShouldDisplayURL();
  }
  GURL url = entry->GetVirtualURL();
  // URL is of the form chrome-native://.... This is not useful for the user.
  // Hide it.
  if (url.SchemeIs(browser_ui::kChromeUINativeScheme)) {
    return false;
  }
  // URL is of the form chrome://....
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return true;
  }
  return ChromeLocationBarModelDelegate::ShouldDisplayURL();
}

void VrShell::OnVoiceResults(const std::u16string& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  GURL url;
  bool input_was_url;
  std::tie(url, input_was_url) =
      autocomplete_controller_->GetUrlFromVoiceInput(result);

  Java_VrShell_loadUrl(env, j_vr_shell_,
                       base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

void VrShell::OnAssetsLoaded(AssetsLoadStatus status,
                             std::unique_ptr<Assets> assets,
                             const base::Version& component_version) {
  ui_->OnAssetsLoaded(status, std::move(assets), component_version);

  if (status == AssetsLoadStatus::kSuccess) {
    VLOG(1) << "Successfully loaded VR assets component";
  } else {
    VLOG(1) << "Failed to load VR assets component";
  }

  ui_finished_loading_ = true;
}

void VrShell::LoadAssets() {
  can_load_new_assets_ = false;
  AssetsLoader::GetInstance()->Load(
      base::BindOnce(&VrShell::OnAssetsLoaded, base::Unretained(this)));
}

void VrShell::OnAssetsComponentReady() {
  can_load_new_assets_ = true;
  // We don't apply updates after the timer expires because that would lead to
  // replacing the user's environment. New updates will be applied when
  // re-entering VR.
  if (waiting_for_assets_component_timer_.IsRunning()) {
    waiting_for_assets_component_timer_.Stop();
    LoadAssets();
  }
}

void VrShell::OnAssetsComponentWaitTimeout() {
  ui_->OnAssetsUnavailable();
  ui_finished_loading_ = true;
}

void VrShell::SetCookieInfo(const CookieInfoList& cookie_info_list) {}

void VrShell::SetPermissionInfo(const PermissionInfoList& permission_info_list,
                                ChosenObjectInfoList chosen_object_info_list) {
  // Here we'll check the current web contents for potentially in-use
  // permissions. Accepting bluetooth is immersive mode  is not currently
  // supported, so we will not check here. Also, the ability to cast is not a
  // page-specific potentiality, so we will not check for this, either.
  for (const auto& info : permission_info_list) {
    switch (info.type) {
      case ContentSettingsType::GEOLOCATION:
        potential_capturing_.location_access_enabled =
            info.setting == CONTENT_SETTING_ALLOW;
        break;
      case ContentSettingsType::MEDIASTREAM_MIC:
        potential_capturing_.audio_capture_enabled =
            info.setting == CONTENT_SETTING_ALLOW;
        break;
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        potential_capturing_.video_capture_enabled =
            info.setting == CONTENT_SETTING_ALLOW;
        break;
      default:
        break;
    }
  }
}

void VrShell::SetIdentityInfo(const IdentityInfo& identity_info) {}

void VrShell::SetPageFeatureInfo(const PageFeatureInfo& info) {
  NOTIMPLEMENTED();
}

void VrShell::AcceptDoffPromptForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::AcceptDoffPromptForTesting,
                                gl_thread_->GetBrowserRenderer()));
}

void VrShell::SetUiExpectingActivityForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint quiescence_timeout_ms) {
  UiTestActivityExpectation ui_expectation;
  ui_expectation.quiescence_timeout_ms = quiescence_timeout_ms;
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(&BrowserRenderer::SetUiExpectingActivityForTesting,
                     gl_thread_->GetBrowserRenderer(), ui_expectation));
}

void VrShell::SaveNextFrameBufferToDiskForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jstring filepath_base) {
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(
          &BrowserRenderer::SaveNextFrameBufferToDiskForTesting,
          gl_thread_->GetBrowserRenderer(),
          base::android::ConvertJavaStringToUTF8(env, filepath_base)));
}

void VrShell::WatchElementForVisibilityStatusForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint element_name,
    jint timeout_ms,
    jboolean visibility) {
  VisibilityChangeExpectation visibility_expectation;
  visibility_expectation.element_name =
      static_cast<UserFriendlyElementName>(element_name);
  visibility_expectation.timeout_ms = timeout_ms;
  visibility_expectation.visibility = visibility;
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(
          &BrowserRenderer::WatchElementForVisibilityStatusForTesting,
          gl_thread_->GetBrowserRenderer(), visibility_expectation));
}

void VrShell::ReportUiOperationResultForTesting(UiTestOperationType action_type,
                                                UiTestOperationResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_reportUiOperationResultForTesting(env, j_vr_shell_,
                                                 static_cast<int>(action_type),
                                                 static_cast<int>(result));
}

void VrShell::PerformControllerActionForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint element_name,
    jint action_type,
    jfloat x,
    jfloat y) {
  ControllerTestInput controller_input;
  controller_input.element_name =
      static_cast<UserFriendlyElementName>(element_name);
  controller_input.action = static_cast<VrControllerTestAction>(action_type);
  controller_input.position = gfx::PointF(x, y);
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(&BrowserRenderer::PerformControllerActionForTesting,
                     gl_thread_->GetBrowserRenderer(), controller_input));
}

void VrShell::PerformKeyboardInputForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint input_type,
    jstring input_string) {
  KeyboardTestInput keyboard_input;
  keyboard_input.action = static_cast<KeyboardTestAction>(input_type);
  keyboard_input.input_text =
      base::android::ConvertJavaStringToUTF8(env, input_string);
  ui_->PerformKeyboardInputForTesting(keyboard_input);
}

std::unique_ptr<PageInfo> VrShell::CreatePageInfo() {
  if (!web_contents_)
    return nullptr;

  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry)
    return nullptr;

  auto page_info = std::make_unique<PageInfo>(
      std::make_unique<ChromePageInfoDelegate>(web_contents_), web_contents_,
      entry->GetVirtualURL());
  page_info->InitializeUiState(this, base::DoNothing());
  return page_info;
}

gfx::AcceleratedWidget VrShell::GetRenderSurface() {
  return surface_window_;
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrShell_Init(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       const JavaParamRef<jobject>& delegate,
                       jboolean for_web_vr,
                       jboolean browsing_disabled,
                       jboolean has_or_can_request_record_audio_permission,
                       jlong gvr_api,
                       jboolean reprojected_rendering,
                       jfloat display_width_meters,
                       jfloat display_height_meters,
                       jint display_width_pixels,
                       jint display_pixel_height,
                       jboolean pause_content,
                       jboolean low_density,
                       jboolean is_standalone_vr_device) {
  UiInitialState ui_initial_state;
  ui_initial_state.browsing_disabled = browsing_disabled;
  ui_initial_state.in_web_vr = for_web_vr;
  ui_initial_state.has_or_can_request_record_audio_permission =
      has_or_can_request_record_audio_permission;
  ui_initial_state.assets_supported = AssetsLoader::AssetsSupported();
  ui_initial_state.is_standalone_vr_device = is_standalone_vr_device;

  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, ui_initial_state,
      VrShellDelegate::GetNativeVrShellDelegate(env, delegate),
      reinterpret_cast<gvr_context*>(gvr_api), reprojected_rendering,
      display_width_meters, display_height_meters, display_width_pixels,
      display_pixel_height, pause_content, low_density));
}

}  // namespace vr
