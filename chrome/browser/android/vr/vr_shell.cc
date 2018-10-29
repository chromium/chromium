// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell.h"

#include <android/native_window_jni.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/android/chrome_feature_list.h"
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
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/metrics/metrics_helper.h"
#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/toolbar_helper.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/vr_web_contents_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
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
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_constants.h"
#include "device/vr/android/gvr/cardboard_gamepad_data_fetcher.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "jni/VrShell_jni.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
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
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {
vr::VrShell* g_vr_shell_instance;

constexpr base::TimeDelta kPollCapturingStateInterval =
    base::TimeDelta::FromSecondsD(0.2);

constexpr base::TimeDelta kAssetsComponentWaitDelay =
    base::TimeDelta::FromSeconds(2);

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
    : web_vr_autopresentation_expected_(
          ui_initial_state.web_vr_autopresentation_expected),
      delegate_provider_(delegate),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reprojected_rendering_(reprojected_rendering),
      display_size_meters_(display_width_meters, display_height_meters),
      display_size_pixels_(display_width_pixels, display_height_pixels),
      gl_surface_created_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      weak_ptr_factory_(this) {
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
  toolbar_ = std::make_unique<ToolbarHelper>(ui_, this);
  autocomplete_controller_ =
      std::make_unique<AutocompleteController>(base::BindRepeating(
          &BrowserUiInterface::SetOmniboxSuggestions, base::Unretained(ui_)));

  gl_thread_->Start();

  if (ui_initial_state.in_web_vr ||
      ui_initial_state.web_vr_autopresentation_expected) {
    UMA_HISTOGRAM_BOOLEAN("VRAutopresentedWebVR",
                          ui_initial_state.web_vr_autopresentation_expected);
  }

  can_load_new_assets_ = AssetsLoader::GetInstance()->ComponentReady();
  if (!can_load_new_assets_) {
    waiting_for_assets_component_timer_.Start(
        FROM_HERE, kAssetsComponentWaitDelay,
        base::BindRepeating(&VrShell::OnAssetsComponentWaitTimeout,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  AssetsLoader::GetInstance()->SetOnComponentReadyCallback(base::BindRepeating(
      &VrShell::OnAssetsComponentReady, weak_ptr_factory_.GetWeakPtr()));
  AssetsLoader::GetInstance()->GetMetricsHelper()->OnEnter(Mode::kVr);

  UpdateVrAssetsComponent(g_browser_process->component_updater());

  auto* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(device::mojom::kServiceName, &geolocation_config_);
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

  // TODO(https://crbug.com/684661): Make SessionMetricsHelper tab-aware and
  // able to track multiple tabs.
  if (web_contents_ && !SessionMetricsHelper::FromWebContents(web_contents_)) {
    SessionMetricsHelper::CreateForWebContents(
        web_contents_,
        webvr_mode_ ? Mode::kWebXrVrPresentation : Mode::kVrBrowsingRegular,
        web_vr_autopresentation_expected_);
  }
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
  if (gvr_gamepad_source_active_) {
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_GVR);
  }

  if (cardboard_gamepad_source_active_) {
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_CARDBOARD);
  }

  delegate_provider_->RemoveDelegate();
  {
    // The GvrLayout is, and must always be, used only on the UI thread, and the
    // GvrApi used for rendering should only be used from the GL thread as it's
    // not thread safe. However, the GvrLayout owns the GvrApi instance, and
    // when it gets shut down it deletes the GvrApi instance with it. Therefore,
    // we need to block shutting down the GvrLayout on stopping our GL thread
    // from using the GvrApi instance.
    // base::Thread::Stop, which is called when destroying the thread, asserts
    // that IO is allowed to prevent jank, but there shouldn't be any concerns
    // regarding jank in this case, because we're switching from 3D to 2D,
    // adding/removing a bunch of Java views, and probably changing device
    // orientation here.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
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

  // Record metrics.
  if (method == NavigationMethod::kOmniboxSuggestionSelected ||
      method == NavigationMethod::kOmniboxUrlEntry) {
    SessionMetricsHelper* metrics_helper =
        SessionMetricsHelper::FromWebContents(web_contents_);
    if (metrics_helper)
      metrics_helper->RecordUrlRequested(url, method);
  }

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

void VrShell::SelectTab(int id, bool incognito) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_selectTab(env, j_vr_shell_, id, incognito);
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

void VrShell::CloseTab(int id, bool incognito) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_closeTab(env, j_vr_shell_, id, incognito);
}

void VrShell::CloseAllTabs() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_closeAllTabs(env, j_vr_shell_);
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
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_CARDBOARD);
    cardboard_gamepad_data_fetcher_ = nullptr;
    cardboard_gamepad_source_active_ = false;
  }

  if (!cardboard_gamepad_source_active_ && enabled) {
    device::GvrDevice* device = delegate_provider_->GetDevice();
    if (!device)
      return;

    device::GamepadDataFetcherManager::GetInstance()->AddFactory(
        new device::CardboardGamepadDataFetcher::Factory(this,
                                                         device->GetId()));
    cardboard_gamepad_source_active_ = true;
    if (pending_cardboard_trigger_) {
      OnTriggerEvent(nullptr, JavaParamRef<jobject>(nullptr), true);
    }
    pending_cardboard_trigger_ = false;
  }
}

void VrShell::ToggleGvrGamepad(bool enabled) {
  // Enable/disable updating gamepad state.
  if (enabled) {
    DCHECK(!gvr_gamepad_source_active_);
    device::GvrDevice* device = delegate_provider_->GetDevice();
    if (!device)
      return;

    device::GamepadDataFetcherManager::GetInstance()->AddFactory(
        new device::GvrGamepadDataFetcher::Factory(this, device->GetId()));
    gvr_gamepad_source_active_ = true;
  } else {
    DCHECK(gvr_gamepad_source_active_);
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_GVR);
    gvr_gamepad_data_fetcher_ = nullptr;
    gvr_gamepad_source_active_ = false;
  }
}

void VrShell::OnTriggerEvent(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             bool touched) {
  // If we are running cardboard, update gamepad state.
  if (cardboard_gamepad_source_active_) {
    device::CardboardGamepadData pad;
    pad.timestamp = cardboard_gamepad_timer_++;
    pad.is_screen_touching = touched;
    if (cardboard_gamepad_data_fetcher_) {
      cardboard_gamepad_data_fetcher_->SetGamepadData(pad);
    }
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

  // exit vr session
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents_);
  if (metrics_helper)
    metrics_helper->SetVRActive(false);
  SetIsInVR(GetNonNativePageWebContents(), false);

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

  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents_);
  if (metrics_helper)
    metrics_helper->SetVRActive(true);
  SetIsInVR(GetNonNativePageWebContents(), true);

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
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents_);
  if (metrics_helper)
    metrics_helper->SetWebVREnabled(enabled);
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::SetWebXrMode,
                                gl_thread_->GetBrowserRenderer(), enabled));
  // We create and dispose a page info in order to get notifed of page
  // permissions.
  CreatePageInfo();
  ui_->SetWebVrMode(enabled);

  if (!webvr_mode_ && !web_vr_autopresentation_expected_) {
    AssetsLoader::GetInstance()->GetMetricsHelper()->OnEnter(Mode::kVrBrowsing);
  } else {
    AssetsLoader::GetInstance()->GetMetricsHelper()->OnEnter(
        Mode::kWebXrVrPresentation);
  }
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
                               const JavaParamRef<jobject>& obj,
                               jobjectArray tabs,
                               jobjectArray incognito_tabs) {
  ui_->RemoveAllTabs();
  size_t len = env->GetArrayLength(incognito_tabs);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_tab(
        env, env->GetObjectArrayElement(incognito_tabs, i));
    TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
    ui_->AddOrUpdateTab(tab->GetAndroidId(), true, tab->GetTitle());
  }

  len = env->GetArrayLength(tabs);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_tab(env, env->GetObjectArrayElement(tabs, i));
    TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
    ui_->AddOrUpdateTab(tab->GetAndroidId(), false, tab->GetTitle());
  }
}

void VrShell::OnTabUpdated(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id,
                           jstring jtitle) {
  base::string16 title;
  base::android::ConvertJavaStringToUTF16(env, jtitle, &title);
  ui_->AddOrUpdateTab(id, incognito, title);
}

void VrShell::OnTabRemoved(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id) {
  ui_->RemoveTab(id, incognito);
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
  base::string16 text;
  base::android::ConvertJavaStringToUTF16(env, jtext, &text);
  gl_thread_->ShowPlatformToast(text);
}

void VrShell::CancelToast(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj) {
  gl_thread_->CancelPlatformToast();
}

void VrShell::ConnectPresentingService(
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&BrowserRenderer::ConnectPresentingService,
                                gl_thread_->GetBrowserRenderer(),
                                std::move(display_info), std::move(options)));
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

void VrShell::GvrDelegateReady(gvr::ViewerType viewer_type) {
  delegate_provider_->SetDelegate(this, viewer_type);
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

void VrShell::LogUnsupportedModeUserMetric(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           int mode) {
  LogUnsupportedModeUserMetric((UiUnsupportedMode)mode);
}

void VrShell::RecordVrStartAction(VrStartAction action) {
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents_);
  if (metrics_helper) {
    metrics_helper->RecordVrStartAction(action);
  }
}

void VrShell::RecordPresentationStartAction(PresentationStartAction action) {
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents_);
  if (metrics_helper)
    metrics_helper->RecordPresentationStartAction(action);
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

void VrShell::LogUnsupportedModeUserMetric(UiUnsupportedMode mode) {
  UMA_HISTOGRAM_ENUMERATION("VR.Shell.EncounteredUnsupportedMode", mode,
                            UiUnsupportedMode::kCount);
}

content::WebContents* VrShell::GetNonNativePageWebContents() const {
  return !web_contents_is_native_page_ ? web_contents_ : nullptr;
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
  if (reason == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission) {
    // Note that we already measure the number of times the user exits VR
    // because of the record audio permission through
    // VR.Shell.EncounteredUnsupportedMode histogram. This histogram measures
    // whether the user chose to proceed to grant the OS record audio permission
    // through the reported Boolean.
    UMA_HISTOGRAM_BOOLEAN("VR.VoiceSearch.RecordAudioOsPermissionPromptChoice",
                          choice == ExitVrPromptChoice::CHOICE_EXIT);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShell_onExitVrRequestResult(env, j_vr_shell_, static_cast<int>(reason),
                                     should_exit);
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

  if (!HasAudioPermission()) {
    OnUnsupportedMode(
        UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
    return;
  }

  if (!speech_recognizer_) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    std::string profile_locale = g_browser_process->GetApplicationLocale();
    speech_recognizer_.reset(new SpeechRecognizer(
        this, ui_,
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
        profile_locale));
  }
  if (active) {
    speech_recognizer_->Start();
    SessionMetricsHelper* metrics_helper =
        SessionMetricsHelper::FromWebContents(web_contents_);
    if (metrics_helper)
      metrics_helper->RecordVoiceSearchStarted();
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

bool VrShell::HasAudioPermission() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_VrShell_hasAudioPermission(env, j_vr_shell_);
}

void VrShell::PollCapturingState() {
  poll_capturing_state_task_.Reset(base::BindRepeating(
      &VrShell::PollCapturingState, base::Unretained(this)));
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

  geolocation_config_->IsHighAccuracyLocationBeingCaptured(base::BindRepeating(
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

void VrShell::UpdateGamepadData(device::GvrGamepadData pad) {
  if (gvr_gamepad_source_active_ != pad.connected)
    ToggleGvrGamepad(pad.connected);

  if (gvr_gamepad_data_fetcher_)
    gvr_gamepad_data_fetcher_->SetGamepadData(pad);
}

void VrShell::RegisterGvrGamepadDataFetcher(
    device::GvrGamepadDataFetcher* fetcher) {
  DVLOG(1) << __FUNCTION__ << "(" << fetcher << ")";
  gvr_gamepad_data_fetcher_ = fetcher;
}

void VrShell::RegisterCardboardGamepadDataFetcher(
    device::CardboardGamepadDataFetcher* fetcher) {
  DVLOG(1) << __FUNCTION__ << "(" << fetcher << ")";
  cardboard_gamepad_data_fetcher_ = fetcher;
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
  if (!entry) {
    return ChromeToolbarModelDelegate::ShouldDisplayURL();
  }
  GURL url = entry->GetVirtualURL();
  // URL is of the form chrome-native://.... This is not useful for the user.
  // Hide it.
  if (url.SchemeIs(chrome::kChromeUINativeScheme)) {
    return false;
  }
  // URL is of the form chrome://....
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return true;
  }
  return ChromeToolbarModelDelegate::ShouldDisplayURL();
}

void VrShell::OnVoiceResults(const base::string16& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  GURL url;
  bool input_was_url;
  std::tie(url, input_was_url) =
      autocomplete_controller_->GetUrlFromVoiceInput(result);

  // TODO(http://crbug.com/817559): If the user is doing a voice search from the
  // new tab page, no metrics data is recorded (including voice search started).
  // Fix this.

  // This should happen before the load to avoid concurency issues.
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents_);
  if (metrics_helper && input_was_url)
    metrics_helper->RecordUrlRequested(url, NavigationMethod::kVoiceSearch);

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

  AssetsLoader::GetInstance()->GetMetricsHelper()->OnAssetsLoaded(
      status, component_version);
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
      case CONTENT_SETTINGS_TYPE_GEOLOCATION:
        potential_capturing_.location_access_enabled =
            info.setting == CONTENT_SETTING_ALLOW;
        break;
      case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
        potential_capturing_.audio_capture_enabled =
            info.setting == CONTENT_SETTING_ALLOW;
        break;
      case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
        potential_capturing_.video_capture_enabled =
            info.setting == CONTENT_SETTING_ALLOW;
        break;
      default:
        break;
    }
  }
}

void VrShell::SetIdentityInfo(const IdentityInfo& identity_info) {}

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

void VrShell::WatchElementForVisibilityChangeForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint element_name,
    jint timeout_ms) {
  VisibilityChangeExpectation visibility_expectation;
  visibility_expectation.element_name =
      static_cast<UserFriendlyElementName>(element_name);
  visibility_expectation.timeout_ms = timeout_ms;
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(
          &BrowserRenderer::WatchElementForVisibilityChangeForTesting,
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

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents_);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  return std::make_unique<PageInfo>(
      this, Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents_), web_contents_,
      entry->GetVirtualURL(), security_info);
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
                       jboolean has_or_can_request_audio_permission,
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
  ui_initial_state.has_or_can_request_audio_permission =
      has_or_can_request_audio_permission;
  ui_initial_state.assets_supported = AssetsLoader::AssetsSupported();
  ui_initial_state.is_standalone_vr_device = is_standalone_vr_device;
  ui_initial_state.create_tabs_view =
      base::FeatureList::IsEnabled(chrome::android::kVrBrowsingTabsView);
  ui_initial_state.use_new_incognito_strings =
      base::FeatureList::IsEnabled(features::kIncognitoStrings);

  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, ui_initial_state,
      VrShellDelegate::GetNativeVrShellDelegate(env, delegate),
      reinterpret_cast<gvr_context*>(gvr_api), reprojected_rendering,
      display_width_meters, display_height_meters, display_width_pixels,
      display_pixel_height, pause_content, low_density));
}

}  // namespace vr
