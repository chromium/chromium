// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_web_contents_delegate_android.h"

#include <stddef.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/hung_renderer_infobar_delegate.h"
#include "chrome/browser/banners/app_banner_manager_android.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/android/bluetooth_chooser_android.h"
#include "chrome/browser/ui/android/infobars/framebust_block_infobar.h"
#include "chrome/browser/ui/blocked_content/popup_blocker.h"
#include "chrome/browser/ui/blocked_content/popup_tracker.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/media_stream_request.h"
#include "jni/TabWebContentsDelegateAndroid_jni.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using blink::mojom::FileChooserParams;
using content::BluetoothChooser;
using content::WebContents;

namespace {

ScopedJavaLocalRef<jobject> JNI_TabWebContentsDelegateAndroid_CreateJavaRectF(
    JNIEnv* env,
    const gfx::RectF& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_TabWebContentsDelegateAndroid_createRectF(env,
                                                        rect.x(),
                                                        rect.y(),
                                                        rect.right(),
                                                        rect.bottom()));
}

ScopedJavaLocalRef<jobject> JNI_TabWebContentsDelegateAndroid_CreateJavaRect(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_TabWebContentsDelegateAndroid_createRect(
          env,
          static_cast<int>(rect.x()),
          static_cast<int>(rect.y()),
          static_cast<int>(rect.right()),
          static_cast<int>(rect.bottom())));
}

infobars::InfoBar* FindHungRendererInfoBar(InfoBarService* infobar_service) {
  DCHECK(infobar_service);
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->AsHungRendererInfoBarDelegate())
      return infobar;
  }
  return nullptr;
}

void ShowFramebustBlockInfobarInternal(content::WebContents* web_contents,
                                       const GURL& url) {
  FramebustBlockInfoBar::Show(
      web_contents,
      std::make_unique<FramebustBlockMessageDelegate>(
          web_contents, url, FramebustBlockMessageDelegate::OutcomeCallback()));
}

}  // anonymous namespace

namespace android {

TabWebContentsDelegateAndroid::TabWebContentsDelegateAndroid(JNIEnv* env,
                                                                   jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

TabWebContentsDelegateAndroid::~TabWebContentsDelegateAndroid() {
  notification_registrar_.RemoveAll();
}

void TabWebContentsDelegateAndroid::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const FileChooserParams& params) {
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          WebContents::FromRenderFrameHost(render_frame_host),
          vr::UiSuppressedElement::kFileChooser)) {
    listener->FileSelectionCanceled();
    return;
  }
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

std::unique_ptr<BluetoothChooser>
TabWebContentsDelegateAndroid::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          WebContents::FromRenderFrameHost(frame),
          vr::UiSuppressedElement::kBluetoothChooser)) {
    return nullptr;
  }
  return std::make_unique<BluetoothChooserAndroid>(frame, event_handler);
}

void TabWebContentsDelegateAndroid::CloseContents(
    WebContents* web_contents) {
  // Prevent dangling registrations assigned to closed web contents.
  if (notification_registrar_.IsRegistered(this,
      chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
      content::Source<WebContents>(web_contents))) {
    notification_registrar_.Remove(this,
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        content::Source<WebContents>(web_contents));
  }

  WebContentsDelegateAndroid::CloseContents(web_contents);
}

bool TabWebContentsDelegateAndroid::ShouldFocusLocationBarByDefault(
    WebContents* source) {
  const content::NavigationEntry* entry =
      source->GetController().GetActiveEntry();
  if (entry) {
    GURL url = entry->GetURL();
    GURL virtual_url = entry->GetVirtualURL();
    if ((url.SchemeIs(chrome::kChromeUINativeScheme) &&
        url.host_piece() == chrome::kChromeUINewTabHost) ||
        (virtual_url.SchemeIs(chrome::kChromeUINativeScheme) &&
        virtual_url.host_piece() == chrome::kChromeUINewTabHost)) {
      return true;
    }
  }
  return false;
}


void TabWebContentsDelegateAndroid::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FIND_RESULT_AVAILABLE, type);
  OnFindResultAvailable(
      content::Source<WebContents>(source).ptr(),
      content::Details<FindNotificationDetails>(details).ptr());
}

blink::WebDisplayMode TabWebContentsDelegateAndroid::GetDisplayMode(
    const WebContents* web_contents) const {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return blink::kWebDisplayModeUndefined;

  return static_cast<blink::WebDisplayMode>(
      Java_TabWebContentsDelegateAndroid_getDisplayMode(env, obj));
}

void TabWebContentsDelegateAndroid::FindReply(
    WebContents* web_contents,
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (!notification_registrar_.IsRegistered(this,
      chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
      content::Source<WebContents>(web_contents))) {
    notification_registrar_.Add(this,
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        content::Source<WebContents>(web_contents));
  }

  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);
  find_tab_helper->HandleFindReply(request_id,
                                   number_of_matches,
                                   selection_rect,
                                   active_match_ordinal,
                                   final_update);
}

void TabWebContentsDelegateAndroid::OnFindResultAvailable(
    WebContents* web_contents,
    const FindNotificationDetails* find_result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jobject> selection_rect =
      JNI_TabWebContentsDelegateAndroid_CreateJavaRect(
          env, find_result->selection_rect());

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_TabWebContentsDelegateAndroid_createFindNotificationDetails(
          env, find_result->number_of_matches(), selection_rect,
          find_result->active_match_ordinal(), find_result->final_update());

  Java_TabWebContentsDelegateAndroid_onFindResultAvailable(env, obj,
                                                           details_object);
}

void TabWebContentsDelegateAndroid::FindMatchRectsReply(
    WebContents* web_contents,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_TabWebContentsDelegateAndroid_createFindMatchRectsDetails(
          env, version, rects.size(),
          JNI_TabWebContentsDelegateAndroid_CreateJavaRectF(env, active_rect));

  // Add the rects
  for (size_t i = 0; i < rects.size(); ++i) {
    Java_TabWebContentsDelegateAndroid_setMatchRectByIndex(
        env, details_object, i,
        JNI_TabWebContentsDelegateAndroid_CreateJavaRectF(env, rects[i]));
  }

  Java_TabWebContentsDelegateAndroid_onFindMatchRectsAvailable(env, obj,
                                                               details_object);
}

content::JavaScriptDialogManager*
TabWebContentsDelegateAndroid::GetJavaScriptDialogManager(
    WebContents* source) {
  if (base::FeatureList::IsEnabled(chrome::android::kTabModalJsDialog) ||
      vr::VrTabHelper::IsInVr(source)) {
    return JavaScriptDialogTabHelper::FromWebContents(source);
  }
  return app_modal::JavaScriptDialogManager::GetInstance();
}

void TabWebContentsDelegateAndroid::AdjustPreviewsStateForNavigation(
    content::WebContents* web_contents,
    content::PreviewsState* previews_state) {
  if (GetDisplayMode(web_contents) != blink::kWebDisplayModeBrowser) {
    *previews_state = content::PREVIEWS_OFF;
  }
}

void TabWebContentsDelegateAndroid::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr);
}

bool TabWebContentsDelegateAndroid::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    content::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

void TabWebContentsDelegateAndroid::SetOverlayMode(bool use_overlay_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  Java_TabWebContentsDelegateAndroid_setOverlayMode(env, obj, use_overlay_mode);
}

bool TabWebContentsDelegateAndroid::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
    return false;
}

WebContents* TabWebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  WindowOpenDisposition disposition = params.disposition;
  if (!source || (disposition != WindowOpenDisposition::CURRENT_TAB &&
                  disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
                  disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
                  disposition != WindowOpenDisposition::OFF_THE_RECORD &&
                  disposition != WindowOpenDisposition::NEW_POPUP &&
                  disposition != WindowOpenDisposition::NEW_WINDOW)) {
    // We can't handle this here.  Give the parent a chance.
    return WebContentsDelegateAndroid::OpenURLFromTab(source, params);
  }

  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  NavigateParams nav_params(profile, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  nav_params.source_contents = source;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = params.user_gesture;
  if ((params.disposition == WindowOpenDisposition::NEW_POPUP ||
       params.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
       params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
       params.disposition == WindowOpenDisposition::NEW_WINDOW) &&
      MaybeBlockPopup(source, base::Optional<GURL>(), &nav_params, &params,
                      blink::mojom::WindowFeatures())) {
    return nullptr;
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    // Only prerender for a current-tab navigation to avoid session storage
    // namespace issues.
    prerender::PrerenderManager::Params prerender_params(&nav_params, source);
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile);
    if (prerender_manager && prerender_manager->MaybeUsePrerenderedPage(
                                 params.url, &prerender_params)) {
      return prerender_params.replaced_contents;
    }
  }

  return WebContentsDelegateAndroid::OpenURLFromTab(source, params);
}

bool TabWebContentsDelegateAndroid::ShouldResumeRequestsForCreatedWindow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;

  return Java_TabWebContentsDelegateAndroid_shouldResumeRequestsForCreatedWindow(
      env, obj);
}

void TabWebContentsDelegateAndroid::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  // No code for this yet.
  DCHECK_NE(disposition, WindowOpenDisposition::SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK_NE(disposition, WindowOpenDisposition::CURRENT_TAB);

  // At this point the |new_contents| is beyond the popup blocker, but we use
  // the same logic for determining if the popup tracker needs to be attached.
  if (source && ConsiderForPopupBlocking(disposition))
    PopupTracker::CreateForWebContents(new_contents.get(), source);

  TabHelpers::AttachTabHelpers(new_contents.get());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  bool handled = false;
  if (!obj.is_null()) {
    ScopedJavaLocalRef<jobject> jsource;
    if (source)
      jsource = source->GetJavaWebContents();
    ScopedJavaLocalRef<jobject> jnew_contents;
    if (new_contents)
      jnew_contents = new_contents->GetJavaWebContents();

    handled = Java_TabWebContentsDelegateAndroid_addNewContents(
        env, obj, jsource, jnew_contents, static_cast<jint>(disposition),
        nullptr, user_gesture);
  }

  if (was_blocked)
    *was_blocked = !handled;

  // When handled is |true|, ownership has been passed to java, which in turn
  // creates a new TabAndroid instance to own the WebContents.
  if (handled)
    new_contents.release();
}

blink::WebSecurityStyle TabWebContentsDelegateAndroid::GetSecurityStyle(
    WebContents* web_contents,
    content::SecurityStyleExplanations* security_style_explanations) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  return security_state::GetSecurityStyle(security_info,
                                          security_style_explanations);
}

void TabWebContentsDelegateAndroid::RequestAppBannerFromDevTools(
    content::WebContents* web_contents) {
  banners::AppBannerManagerAndroid* manager =
      banners::AppBannerManagerAndroid::FromWebContents(web_contents);
  DCHECK(manager);
  manager->RequestAppBanner(web_contents->GetLastCommittedURL(), true);
}

void TabWebContentsDelegateAndroid::OnDidBlockFramebust(
    content::WebContents* web_contents,
    const GURL& url) {
  ShowFramebustBlockInfobarInternal(web_contents, url);
}

void TabWebContentsDelegateAndroid::UpdateUserGestureCarryoverInfo(
    content::WebContents* web_contents) {
  auto* intercept_navigation_delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (intercept_navigation_delegate)
    intercept_navigation_delegate->UpdateLastUserGestureCarryoverTimestamp();
}

std::unique_ptr<content::WebContents>
TabWebContentsDelegateAndroid::SwapWebContents(
    content::WebContents* old_contents,
    std::unique_ptr<content::WebContents> new_contents,
    bool did_start_load,
    bool did_finish_load) {
  // TODO(crbug.com/836409): TabLoadTracker should not rely on being notified
  // directly about tab contents swaps.
  resource_coordinator::TabLoadTracker::Get()->SwapTabContents(
      old_contents, new_contents.get());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabWebContentsDelegateAndroid_swapWebContents(
      env, GetJavaDelegate(env), new_contents->GetJavaWebContents(),
      did_start_load, did_finish_load);
  new_contents.release();
  return base::WrapUnique(old_contents);
}

}  // namespace android

void JNI_TabWebContentsDelegateAndroid_OnRendererUnresponsive(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHungRendererInfoBar)) {
    return;
  }

  content::WebContents* web_contents =
        content::WebContents::FromJavaWebContents(java_web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  DCHECK(!FindHungRendererInfoBar(infobar_service));
  HungRendererInfoBarDelegate::Create(
      infobar_service, web_contents->GetMainFrame()->GetProcess());
}

void JNI_TabWebContentsDelegateAndroid_OnRendererResponsive(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
          content::WebContents::FromJavaWebContents(java_web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobars::InfoBar* hung_renderer_infobar =
      FindHungRendererInfoBar(infobar_service);
  if (!hung_renderer_infobar)
    return;

  hung_renderer_infobar->delegate()
      ->AsHungRendererInfoBarDelegate()
      ->OnRendererResponsive();
  infobar_service->RemoveInfoBar(hung_renderer_infobar);
}

jboolean JNI_TabWebContentsDelegateAndroid_IsCapturingAudio(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingAudio(web_contents);
}

jboolean JNI_TabWebContentsDelegateAndroid_IsCapturingVideo(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingVideo(web_contents);
}

jboolean JNI_TabWebContentsDelegateAndroid_IsCapturingScreen(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  return indicator->IsCapturingDesktop(web_contents);
}

void JNI_TabWebContentsDelegateAndroid_NotifyStopped(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  indicator->NotifyStopped(web_contents);
}

void JNI_TabWebContentsDelegateAndroid_ShowFramebustBlockInfoBar(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& java_web_contents,
    const JavaParamRef<jstring>& java_url) {
  GURL url(base::android::ConvertJavaStringToUTF16(env, java_url));
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  ShowFramebustBlockInfobarInternal(web_contents, url);
}
