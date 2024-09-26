// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_contents_delegate.h"

#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_javascript_dialog_manager.h"
#include "android_webview/browser/aw_permission_manager.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/permission/media_access_permission_request.h"
#include "android_webview/browser/permission/permission_request_handler.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwWebContentsDelegate_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using blink::mojom::FileChooserFileInfo;
using blink::mojom::FileChooserFileInfoPtr;
using blink::mojom::FileChooserParams;
using content::WebContents;

namespace android_webview {

AwWebContentsDelegate::AwWebContentsDelegate(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj)
    : WebContentsDelegateAndroid(env, obj), is_fullscreen_(false) {}

AwWebContentsDelegate::~AwWebContentsDelegate() = default;

void AwWebContentsDelegate::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  AwContents* aw_contents = AwContents::FromWebContents(source);
  if (!aw_contents)
    return;

  content::RenderProcessHost* render_process_host =
      render_widget_host->GetProcess();
  if (render_process_host->IsInitializedAndNotDead()) {
    aw_contents->RendererUnresponsive(render_widget_host->GetProcess());
    hang_monitor_restarter.Run();
  }
}

void AwWebContentsDelegate::RendererResponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  AwContents* aw_contents = AwContents::FromWebContents(source);
  if (!aw_contents)
    return;

  content::RenderProcessHost* render_process_host =
      render_widget_host->GetProcess();
  if (render_process_host->IsInitializedAndNotDead()) {
    aw_contents->RendererResponsive(render_widget_host->GetProcess());
  }
}

content::JavaScriptDialogManager*
AwWebContentsDelegate::GetJavaScriptDialogManager(WebContents* source) {
  static base::NoDestructor<AwJavaScriptDialogManager>
      javascript_dialog_manager;
  return javascript_dialog_manager.get();
}

void AwWebContentsDelegate::FindReply(WebContents* web_contents,
                                      int request_id,
                                      int number_of_matches,
                                      const gfx::Rect& selection_rect,
                                      int active_match_ordinal,
                                      bool final_update) {
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents)
    return;

  aw_contents->GetFindHelper()->HandleFindReply(
      request_id, number_of_matches, active_match_ordinal, final_update);
}

void AwWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const FileChooserParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (!java_delegate.obj()) {
    listener->FileSelectionCanceled();
    return;
  }

  // Only allow Open, OpenMultiple and UploadFolder for pre-FSA code.
  // TODO(b/364980165): Add check for
  // base::android::BuildInfo::GetInstance()->target_sdk_version()
  if (!base::FeatureList::IsEnabled(features::kWebViewFileSystemAccess) &&
      params.mode != FileChooserParams::Mode::kOpen &&
      params.mode != FileChooserParams::Mode::kOpenMultiple &&
      params.mode != FileChooserParams::Mode::kUploadFolder) {
    listener->FileSelectionCanceled();
    return;
  }
  DCHECK(!file_select_listener_)
      << "Multiple concurrent FileChooser requests are not supported.";
  file_select_listener_ = std::move(listener);
  Java_AwWebContentsDelegate_runFileChooser(
      env, java_delegate, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), params.mode,
      ConvertUTF16ToJavaString(env,
                               base::JoinString(params.accept_types, u",")),
      params.title.empty() ? nullptr
                           : ConvertUTF16ToJavaString(env, params.title),
      params.default_file_name.empty()
          ? nullptr
          : ConvertUTF8ToJavaString(env, params.default_file_name.value()),
      params.use_media_capture);
}

bool AwWebContentsDelegate::UseFileChooserForFileSystemAccess() const {
  return true;
}

WebContents* AwWebContentsDelegate::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  JNIEnv* env = AttachCurrentThread();

  bool is_dialog = disposition == WindowOpenDisposition::NEW_POPUP;
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  bool create_popup = false;

  if (java_delegate.obj()) {
    create_popup = Java_AwWebContentsDelegate_addNewContents(
        env, java_delegate, is_dialog, user_gesture);
  }

  if (create_popup) {
    // The embedder would like to display the popup and we will receive
    // a callback from them later with an AwContents to use to display
    // it. The source AwContents takes ownership of the new WebContents
    // until then, and when the callback is made we will swap the WebContents
    // out into the new AwContents.
    WebContents* raw_new_contents = new_contents.get();
    AwContents::FromWebContents(source)->SetPendingWebContentsForPopup(
        std::move(new_contents));
    // It's possible that SetPendingWebContentsForPopup deletes |new_contents|,
    // but it only does so asynchronously, so it's safe to use a raw pointer
    // here.
    // Hide the WebContents for the pop up now, we will show it again
    // when the user calls us back with an AwContents to use to show it.
    raw_new_contents->WasHidden();
  } else {
    // The embedder has forgone their chance to display this popup
    // window, so we're done with the WebContents now. We use
    // DeleteSoon as WebContentsImpl may call methods on |new_contents|
    // after this method returns.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(new_contents));
  }

  if (was_blocked) {
    *was_blocked = !create_popup;
  }
  return nullptr;
}

void AwWebContentsDelegate::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_AwWebContentsDelegate_navigationStateChanged(env, java_delegate,
                                                      changed_flags);
  }
}

// Notifies the delegate about the creation of a new WebContents. This
// typically happens when popups are created.
void AwWebContentsDelegate::WebContentsCreated(
    WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  // Intentionally left empty to override implementation in superclasses.
}

void AwWebContentsDelegate::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_AwWebContentsDelegate_closeContents(env, java_delegate);
  }
}

void AwWebContentsDelegate::ActivateContents(WebContents* contents) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_AwWebContentsDelegate_activateContents(env, java_delegate);
  }
}

void AwWebContentsDelegate::LoadingStateChanged(WebContents* source,
                                                bool to_different_document) {
  // Page title may have changed, need to inform the embedder.
  // |source| may be null if loading has started.
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.obj()) {
    Java_AwWebContentsDelegate_loadingStateChanged(env, java_delegate);
  }
}

bool AwWebContentsDelegate::ShouldResumeRequestsForCreatedWindow() {
  // Always return false here since we need to defer loading the created window
  // until after we have attached a new delegate to the new webcontents (which
  // happens asynchronously).
  return false;
}

void AwWebContentsDelegate::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        nullptr);
    return;
  }
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  bool allow_file_access_from_file_urls =
      aw_settings->GetAllowFileAccessFromFileURLs();
  aw_contents->GetPermissionRequestHandler()->SendRequest(
      std::make_unique<MediaAccessPermissionRequest>(
          request, std::move(callback),
          *AwBrowserContext::FromWebContents(web_contents)
               ->GetPermissionControllerDelegate(),
          allow_file_access_from_file_urls));
}

bool AwWebContentsDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (!base::FeatureList::IsEnabled(features::kWebViewEnumerateDevicesCache)) {
    return false;
  }
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return false;
  }
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents);
  if (!aw_settings->GetAllowFileAccessFromFileURLs() &&
      security_origin.scheme() == url::kFileScheme) {
    return false;
  }
  AwPermissionManager* pm = AwBrowserContext::FromWebContents(web_contents)
                                ->GetPermissionControllerDelegate();
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return pm->ShouldShowEnumerateDevicesAudioLabels(security_origin);
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return pm->ShouldShowEnumerateDevicesVideoLabels(security_origin);
    default:
      return false;
  }
}

void AwWebContentsDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  WebContentsDelegateAndroid::EnterFullscreenModeForTab(requesting_frame,
                                                        options);
  is_fullscreen_ = true;
  requesting_frame->GetRenderViewHost()
      ->GetWidget()
      ->SynchronizeVisualProperties();
}

void AwWebContentsDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  WebContentsDelegateAndroid::ExitFullscreenModeForTab(web_contents);
  is_fullscreen_ = false;
  web_contents->GetRenderViewHost()->GetWidget()->SynchronizeVisualProperties();
}

bool AwWebContentsDelegate::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

void AwWebContentsDelegate::UpdateUserGestureCarryoverInfo(
    content::WebContents* web_contents) {
  auto* intercept_navigation_delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (intercept_navigation_delegate)
    intercept_navigation_delegate->OnResourceRequestWithGesture();
}

bool AwWebContentsDelegate::IsBackForwardCacheSupported(
    content::WebContents& web_contents) {
  AwSettings* aw_settings = AwSettings::FromWebContents(&web_contents);
  return base::FeatureList::IsEnabled(features::kWebViewBackForwardCache) ||
         aw_settings->IsBackForwardCacheEnabled();
}

content::PreloadingEligibility AwWebContentsDelegate::IsPrerender2Supported(
    content::WebContents& web_contents) {
  AwSettings* aw_settings = AwSettings::FromWebContents(&web_contents);
  if (aw_settings->IsPrerender2Allowed()) {
    return content::PreloadingEligibility::kEligible;
  }

  return content::PreloadingEligibility::kPreloadingUnsupportedByWebContents;
}

content::NavigationController::UserAgentOverrideOption
AwWebContentsDelegate::ShouldOverrideUserAgentForPrerender2() {
  // For WebView, always use the user agent override, which is set every time
  // the user agent in AwSettings is modified.
  return content::NavigationController::UA_OVERRIDE_TRUE;
}

bool AwWebContentsDelegate::ShouldAllowPartialParamMismatchOfPrerender2(
    content::NavigationHandle& navigation_handle) {
  // We relax initiator checks on WebView first, but continue to discuss.
  //
  // TODO(https://crbug.com/340416082): Relax initiator check for all platforms.

  // `ui::PAGE_TRANSITION_FROM_API` bit distinguishes that the activation
  // navigation is triggered by `WebView.loadUrl()`.
  bool ret =
      navigation_handle.GetPageTransition() & ui::PAGE_TRANSITION_FROM_API;
  if (ret) {
    CHECK(!navigation_handle.GetInitiatorFrameToken().has_value());
    CHECK(!navigation_handle.GetInitiatorOrigin().has_value());
  }
  return ret;
}

scoped_refptr<content::FileSelectListener>
AwWebContentsDelegate::TakeFileSelectListener() {
  return std::move(file_select_listener_);
}

static void JNI_AwWebContentsDelegate_FilesSelectedInChooser(
    JNIEnv* env,
    jint process_id,
    jint render_id,
    jint mode_flags,
    const JavaParamRef<jobjectArray>& file_paths,
    const JavaParamRef<jobjectArray>& display_names) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, render_id);
  auto* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* delegate =
      static_cast<AwWebContentsDelegate*>(web_contents->GetDelegate());
  if (!delegate)
    return;
  scoped_refptr<content::FileSelectListener> listener =
      delegate->TakeFileSelectListener();

  if (!file_paths.obj()) {
    listener->FileSelectionCanceled();
    return;
  }

  std::vector<std::string> file_path_str;
  std::vector<std::u16string> display_name_str;
  // Note file_paths maybe NULL, but this will just yield a zero-length vector.
  base::android::AppendJavaStringArrayToStringVector(env, file_paths,
                                                     &file_path_str);
  base::android::AppendJavaStringArrayToStringVector(env, display_names,
                                                     &display_name_str);
  std::vector<FileChooserFileInfoPtr> files;
  files.reserve(file_path_str.size());
  for (size_t i = 0; i < file_path_str.size(); ++i) {
    GURL url(file_path_str[i]);
    if (!url.is_valid()) {
      LOG(ERROR) << "The file choice request has an invalid Uri: "
                 << file_path_str[i];
      continue;
    }
    base::FilePath path;
    if (url.SchemeIsFile()) {
      if (!net::FileURLToFilePath(url, &path))
        continue;
    } else {
      path = base::FilePath(file_path_str[i]);
    }
    auto file_info = blink::mojom::NativeFileInfo::New();
    file_info->file_path = path;
    if (!display_name_str[i].empty())
      file_info->display_name = display_name_str[i];
    files.push_back(FileChooserFileInfo::NewNativeFile(std::move(file_info)));
  }
  base::FilePath base_dir;
  FileChooserParams::Mode mode;
  // We'd like to set |base_dir| to a folder which a user selected. But it's
  // impossible with WebChromeClient API in the current Android.
  if (mode_flags == static_cast<int>(FileChooserParams::Mode::kOpenMultiple)) {
    mode = FileChooserParams::Mode::kOpenMultiple;
  } else {
    mode = FileChooserParams::Mode::kOpen;
  }
  DVLOG(0) << "File Chooser result: mode = " << mode
           << ", file paths = " << base::JoinString(file_path_str, ":");
  listener->FileSelected(std::move(files), base_dir, mode);
}

}  // namespace android_webview
