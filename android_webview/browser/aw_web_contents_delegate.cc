// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_contents_delegate.h"

#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_javascript_dialog_manager.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/permission/media_access_permission_request.h"
#include "android_webview/browser/permission/permission_request_handler.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "jni/AwWebContentsDelegate_jni.h"
#include "net/base/escape.h"

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

namespace {

// WARNING: these constants are exposed in the public interface Java side, so
// must remain in sync with what clients are expecting.
const int kFileChooserModeOpenMultiple = 1 << 0;
const int kFileChooserModeOpenFolder = 1 << 1;

base::LazyInstance<AwJavaScriptDialogManager>::Leaky
    g_javascript_dialog_manager = LAZY_INSTANCE_INITIALIZER;
}

AwWebContentsDelegate::AwWebContentsDelegate(JNIEnv* env, jobject obj)
    : WebContentsDelegateAndroid(env, obj), is_fullscreen_(false) {}

AwWebContentsDelegate::~AwWebContentsDelegate() {}

content::JavaScriptDialogManager*
AwWebContentsDelegate::GetJavaScriptDialogManager(WebContents* source) {
  return g_javascript_dialog_manager.Pointer();
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

void AwWebContentsDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  // Android webview intercepts download in its resource dispatcher host
  // delegate, so should not reach here.
  NOTREACHED();
  callback.Run(false);
}

void AwWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const FileChooserParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (!java_delegate.obj()) {
    listener->FileSelectionCanceled();
    return;
  }

  int mode_flags = 0;
  if (params.mode == FileChooserParams::Mode::kOpenMultiple) {
    mode_flags |= kFileChooserModeOpenMultiple;
  } else if (params.mode == FileChooserParams::Mode::kUploadFolder) {
    // Folder implies multiple in Chrome.
    mode_flags |= kFileChooserModeOpenMultiple | kFileChooserModeOpenFolder;
  } else if (params.mode == FileChooserParams::Mode::kSave) {
    // Save not supported, so cancel it.
    listener->FileSelectionCanceled();
    return;
  } else {
    DCHECK_EQ(FileChooserParams::Mode::kOpen, params.mode);
  }
  DCHECK(!file_select_listener_)
      << "Multiple concurrent FileChooser requests are not supported.";
  file_select_listener_ = std::move(listener);
  Java_AwWebContentsDelegate_runFileChooser(
      env, java_delegate, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), mode_flags,
      ConvertUTF16ToJavaString(
          env, base::JoinString(params.accept_types, base::ASCIIToUTF16(","))),
      params.title.empty() ? nullptr
                           : ConvertUTF16ToJavaString(env, params.title),
      params.default_file_name.empty()
          ? nullptr
          : ConvertUTF8ToJavaString(env, params.default_file_name.value()),
      params.use_media_capture);
}

void AwWebContentsDelegate::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
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
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    std::move(new_contents));
  }

  if (was_blocked) {
    *was_blocked = !create_popup;
  }
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
  AwContentsIoThreadClient::RegisterPendingContents(new_contents);
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
    std::move(callback).Run(content::MediaStreamDevices(),
                            content::MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN,
                            std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  aw_contents->GetPermissionRequestHandler()->SendRequest(
      std::unique_ptr<AwPermissionRequestDelegate>(
          new MediaAccessPermissionRequest(request, std::move(callback))));
}

void AwWebContentsDelegate::EnterFullscreenModeForTab(
    content::WebContents* web_contents,
    const GURL& origin,
    const blink::WebFullscreenOptions& options) {
  WebContentsDelegateAndroid::EnterFullscreenModeForTab(web_contents, origin,
                                                        options);
  is_fullscreen_ = true;
  web_contents->GetRenderViewHost()->GetWidget()->SynchronizeVisualProperties();
}

void AwWebContentsDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  WebContentsDelegateAndroid::ExitFullscreenModeForTab(web_contents);
  is_fullscreen_ = false;
  web_contents->GetRenderViewHost()->GetWidget()->SynchronizeVisualProperties();
}

bool AwWebContentsDelegate::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) const {
  return is_fullscreen_;
}

void AwWebContentsDelegate::UpdateUserGestureCarryoverInfo(
    content::WebContents* web_contents) {
  auto* intercept_navigation_delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (intercept_navigation_delegate)
    intercept_navigation_delegate->UpdateLastUserGestureCarryoverTimestamp();
}

std::unique_ptr<content::FileSelectListener>
AwWebContentsDelegate::TakeFileSelectListener() {
  return std::move(file_select_listener_);
}

static void JNI_AwWebContentsDelegate_FilesSelectedInChooser(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
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
  std::unique_ptr<content::FileSelectListener> listener =
      delegate->TakeFileSelectListener();

  if (!file_paths.obj()) {
    listener->FileSelectionCanceled();
    return;
  }

  std::vector<std::string> file_path_str;
  std::vector<base::string16> display_name_str;
  // Note file_paths maybe NULL, but this will just yield a zero-length vector.
  base::android::AppendJavaStringArrayToStringVector(env, file_paths,
                                                     &file_path_str);
  base::android::AppendJavaStringArrayToStringVector(env, display_names,
                                                     &display_name_str);
  std::vector<FileChooserFileInfoPtr> files;
  files.reserve(file_path_str.size());
  for (size_t i = 0; i < file_path_str.size(); ++i) {
    GURL url(file_path_str[i]);
    if (!url.is_valid())
      continue;
    base::FilePath path(
        url.SchemeIsFile()
            ? net::UnescapeURLComponent(
                  url.path(), net::UnescapeRule::SPACES |
                                  net::UnescapeRule::
                                      URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS)
            : file_path_str[i]);
    auto file_info = blink::mojom::NativeFileInfo::New();
    file_info->file_path = path;
    if (!display_name_str[i].empty())
      file_info->display_name = display_name_str[i];
    files.push_back(FileChooserFileInfo::NewNativeFile(std::move(file_info)));
  }
  FileChooserParams::Mode mode;
  if (mode_flags & kFileChooserModeOpenFolder) {
    mode = FileChooserParams::Mode::kUploadFolder;
  } else if (mode_flags & kFileChooserModeOpenMultiple) {
    mode = FileChooserParams::Mode::kOpenMultiple;
  } else {
    mode = FileChooserParams::Mode::kOpen;
  }
  DVLOG(0) << "File Chooser result: mode = " << mode
           << ", file paths = " << base::JoinString(file_path_str, ":");
  listener->FileSelected(std::move(files), mode);
}

}  // namespace android_webview
