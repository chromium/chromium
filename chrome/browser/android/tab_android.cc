// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "chrome/android/chrome_jni_headers/TabImpl_jni.h"
#include "chrome/browser/android/background_tab_manager.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/browser/android/tab_printer.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/prerender/browser/prerender_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_request_body_android.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using chrome::android::BackgroundTabManager;
using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;

namespace {

class TabAndroidHelper : public content::WebContentsUserData<TabAndroidHelper> {
 public:
  static void SetTabForWebContents(WebContents* contents,
                                   TabAndroid* tab_android) {
    content::WebContentsUserData<TabAndroidHelper>::CreateForWebContents(
        contents);
    content::WebContentsUserData<TabAndroidHelper>::FromWebContents(contents)
        ->tab_android_ = tab_android;
  }

  static TabAndroid* FromWebContents(const WebContents* contents) {
    TabAndroidHelper* helper =
        static_cast<TabAndroidHelper*>(contents->GetUserData(UserDataKey()));
    return helper ? helper->tab_android_ : nullptr;
  }

  explicit TabAndroidHelper(content::WebContents*) {}

 private:
  friend class content::WebContentsUserData<TabAndroidHelper>;

  TabAndroid* tab_android_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabAndroidHelper)

}  // namespace

TabAndroid* TabAndroid::FromWebContents(
    const content::WebContents* web_contents) {
  return TabAndroidHelper::FromWebContents(web_contents);
}

TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, const JavaRef<jobject>& obj) {
  return reinterpret_cast<TabAndroid*>(Java_TabImpl_getNativePtr(env, obj));
}

void TabAndroid::AttachTabHelpers(content::WebContents* web_contents) {
  DCHECK(web_contents);

  TabHelpers::AttachTabHelpers(web_contents);
}

TabAndroid::TabAndroid(JNIEnv* env, const JavaRef<jobject>& obj)
    : weak_java_tab_(env, obj),
      session_window_id_(SessionID::InvalidValue()),
      content_layer_(cc::Layer::Create()),
      synced_tab_delegate_(new browser_sync::SyncedTabDelegateAndroid(this)) {
  Java_TabImpl_setNativePtr(env, obj, reinterpret_cast<intptr_t>(this));
}

TabAndroid::~TabAndroid() {
  GetContentLayer()->RemoveAllChildren();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabImpl_clearNativePtr(env, weak_java_tab_.get(env));
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return weak_java_tab_.get(env);
}

scoped_refptr<cc::Layer> TabAndroid::GetContentLayer() const {
  return content_layer_;
}

int TabAndroid::GetAndroidId() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabImpl_getId(env, weak_java_tab_.get(env));
}

bool TabAndroid::IsNativePage() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabImpl_isNativePage(env, weak_java_tab_.get(env));
}

base::string16 TabAndroid::GetTitle() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_title =
      Java_TabImpl_getTitle(env, weak_java_tab_.get(env));
  return java_title ? base::android::ConvertJavaStringToUTF16(java_title)
                    : base::string16();
}

GURL TabAndroid::GetURL() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<GURL> gurl = url::GURLAndroid::ToNativeGURL(
      env, Java_TabImpl_getUrl(env, weak_java_tab_.get(env)));
  return std::move(*gurl);
}

bool TabAndroid::IsUserInteractable() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabImpl_isUserInteractable(env, weak_java_tab_.get(env));
}

Profile* TabAndroid::GetProfile() const {
  if (!web_contents())
    return nullptr;

  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

sync_sessions::SyncedTabDelegate* TabAndroid::GetSyncedTabDelegate() const {
  return synced_tab_delegate_.get();
}

void TabAndroid::DeleteFrozenNavigationEntries(
    const WebContentsState::DeletionPredicate& predicate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabImpl_deleteNavigationEntriesFromFrozenState(
      env, weak_java_tab_.get(env), reinterpret_cast<intptr_t>(&predicate));
}

void TabAndroid::SetWindowSessionID(SessionID window_id) {
  session_window_id_ = window_id;

  if (!web_contents())
    return;

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(web_contents());
  session_tab_helper->SetWindowID(session_window_id_);
}

std::unique_ptr<content::WebContents> TabAndroid::SwapWebContents(
    std::unique_ptr<content::WebContents> new_contents,
    bool did_start_load,
    bool did_finish_load) {
  content::WebContents* old_contents = web_contents_.get();
  // TODO(crbug.com/836409): TabLoadTracker should not rely on being notified
  // directly about tab contents swaps.
  resource_coordinator::TabLoadTracker::Get()->SwapTabContents(
      old_contents, new_contents.get());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabImpl_swapWebContents(env, weak_java_tab_.get(env),
                               new_contents->GetJavaWebContents(),
                               did_start_load, did_finish_load);
  DCHECK_EQ(web_contents_, new_contents);
  new_contents.release();
  return base::WrapUnique(old_contents);
}

bool TabAndroid::IsCustomTab() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabImpl_isCustomTab(env, weak_java_tab_.get(env));
}

bool TabAndroid::IsHidden() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TabImpl_isHidden(env, weak_java_tab_.get(env));
}

void TabAndroid::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void TabAndroid::InitWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean incognito,
    jboolean is_background_tab,
    const JavaParamRef<jobject>& jweb_contents,
    jint jparent_tab_id,
    const JavaParamRef<jobject>& jweb_contents_delegate,
    const JavaParamRef<jobject>& jcontext_menu_populator_factory) {
  web_contents_.reset(content::WebContents::FromJavaWebContents(jweb_contents));
  DCHECK(web_contents_.get());

  TabAndroidHelper::SetTabForWebContents(web_contents(), this);
  web_contents_delegate_ =
      std::make_unique<android::TabWebContentsDelegateAndroid>(
          env, jweb_contents_delegate);
  web_contents()->SetDelegate(web_contents_delegate_.get());

  AttachTabHelpers(web_contents_.get());

  PropagateHideFutureNavigationsToHistoryTabHelper();

  SetWindowSessionID(session_window_id_);

  ContextMenuHelper::FromWebContents(web_contents())
      ->SetPopulatorFactory(jcontext_menu_populator_factory);

  synced_tab_delegate_->SetWebContents(web_contents(), jparent_tab_id);

  // Verify that the WebContents this tab represents matches the expected
  // off the record state.
  CHECK_EQ(GetProfile()->IsOffTheRecord(), incognito);

  if (is_background_tab) {
    BackgroundTabManager::GetInstance()->RegisterBackgroundTab(web_contents(),
                                                               GetProfile());
  }
  content_layer_->InsertChild(web_contents_->GetNativeView()->GetLayer(), 0);

  // Shows a warning notification for dangerous flags in about:flags.
  chrome::ShowBadFlagsPrompt(web_contents());
}

void TabAndroid::UpdateDelegates(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents_delegate,
    const JavaParamRef<jobject>& jcontext_menu_populator_factory) {
  ContextMenuHelper::FromWebContents(web_contents())
      ->SetPopulatorFactory(jcontext_menu_populator_factory);
  web_contents_delegate_ =
      std::make_unique<android::TabWebContentsDelegateAndroid>(
          env, jweb_contents_delegate);
  web_contents()->SetDelegate(web_contents_delegate_.get());
}

namespace {
void WillRemoveWebContentsFromTab(content::WebContents* contents) {
  DCHECK(contents);

  if (contents->GetNativeView())
    contents->GetNativeView()->GetLayer()->RemoveFromParent();

  contents->SetDelegate(nullptr);
}
}  // namespace

void TabAndroid::DestroyWebContents(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  WillRemoveWebContentsFromTab(web_contents());

  // Terminate the renderer process if this is the last tab.
  // If there's no unload listener, FastShutdownIfPossible kills the
  // renderer process. Otherwise, we go with the slow path where renderer
  // process shuts down itself when ref count becomes 0.
  // This helps the render process exit quickly which avoids some issues
  // during shutdown. See https://codereview.chromium.org/146693011/
  // and http://crbug.com/338709 for details.
  content::RenderProcessHost* process =
      web_contents()->GetMainFrame()->GetProcess();
  if (process)
    process->FastShutdownIfPossible(1, false);

  web_contents_.reset();

  synced_tab_delegate_->ResetWebContents();
}

void TabAndroid::ReleaseWebContents(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  WillRemoveWebContentsFromTab(web_contents());

  // Ownership of |released_contents| is assumed by the code that initiated the
  // release.
  content::WebContents* released_contents = web_contents_.release();

  // Remove the link from the native WebContents to |this|, since the
  // lifetimes of the two objects are no longer intertwined.
  TabAndroidHelper::SetTabForWebContents(released_contents, nullptr);

  synced_tab_delegate_->ResetWebContents();
}

void TabAndroid::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  gfx::Size size(width, height);
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(size);
}

TabAndroid::TabLoadStatus TabAndroid::LoadUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jobject>& j_initiator_origin,
    const JavaParamRef<jstring>& j_extra_headers,
    const JavaParamRef<jobject>& j_post_data,
    jint page_transition,
    const JavaParamRef<jstring>& j_referrer_url,
    jint referrer_policy,
    jboolean is_renderer_initiated,
    jboolean should_replace_current_entry,
    jboolean has_user_gesture,
    jboolean should_clear_history_list,
    jlong input_start_timestamp,
    jlong intent_received_timestamp) {
  if (!web_contents())
    return PAGE_LOAD_FAILED;

  if (url.is_null())
    return PAGE_LOAD_FAILED;

  GURL gurl(base::android::ConvertJavaStringToUTF8(env, url));
  if (gurl.is_empty())
    return PAGE_LOAD_FAILED;

  GURL fixed_url(
      url_formatter::FixupURL(gurl.possibly_invalid_spec(), std::string()));
  if (!fixed_url.is_valid())
    return PAGE_LOAD_FAILED;

  if (!HandleNonNavigationAboutURL(fixed_url)) {
    // Record UMA "ShowHistory" here. That way it'll pick up both user
    // typing chrome://history as well as selecting from the drop down menu.
    if (fixed_url.spec() == chrome::kChromeUIHistoryURL) {
      base::RecordAction(base::UserMetricsAction("ShowHistory"));
    }

    content::NavigationController::LoadURLParams load_params(fixed_url);
    if (j_extra_headers) {
      load_params.extra_headers = base::android::ConvertJavaStringToUTF8(
          env,
          j_extra_headers);
    }
    if (j_post_data) {
      load_params.load_type =
          content::NavigationController::LOAD_TYPE_HTTP_POST;
      load_params.post_data =
          content::ExtractResourceRequestBodyFromJavaObject(env, j_post_data);
    }
    load_params.transition_type =
        ui::PageTransitionFromInt(page_transition);
    if (j_referrer_url) {
      load_params.referrer = content::Referrer(
          GURL(base::android::ConvertJavaStringToUTF8(env, j_referrer_url)),
          content::Referrer::ConvertToPolicy(referrer_policy));
    }
    if (j_initiator_origin) {
      load_params.initiator_origin =
          url::Origin::FromJavaObject(j_initiator_origin);
    }
    load_params.is_renderer_initiated = is_renderer_initiated;
    load_params.should_replace_current_entry = should_replace_current_entry;
    load_params.has_user_gesture = has_user_gesture;
    load_params.should_clear_history_list = should_clear_history_list;
    if (input_start_timestamp != 0) {
      load_params.input_start =
          base::TimeTicks::FromUptimeMillis(input_start_timestamp);
    } else if (intent_received_timestamp != 0) {
      load_params.input_start =
          base::TimeTicks::FromUptimeMillis(intent_received_timestamp);
    }
    web_contents()->GetController().LoadURLWithParams(load_params);
  }
  return DEFAULT_PAGE_LOAD;
}

void TabAndroid::SetActiveNavigationEntryTitleForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jtitle) {
  DCHECK(web_contents());

  base::string16 title;
  if (jtitle)
    title = base::android::ConvertJavaStringToUTF16(env, jtitle);

  std::string url;
  if (jurl)
    url = base::android::ConvertJavaStringToUTF8(env, jurl);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (entry && url == entry->GetVirtualURL().spec())
    entry->SetTitle(title);
}

void TabAndroid::LoadOriginalImage(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetFocusedFrame();
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> renderer;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&renderer);
  renderer->RequestReloadImageForContextNode();
}

void TabAndroid::SetAddApi2TransitionToFutureNavigations(JNIEnv* env,
                                                         jboolean should_add) {
  should_add_api2_transition_to_future_navigations_ = should_add;
}

scoped_refptr<content::DevToolsAgentHost> TabAndroid::GetDevToolsAgentHost() {
  return devtools_host_;
}

void TabAndroid::SetHideFutureNavigations(JNIEnv* env, jboolean hide) {
  if (hide_future_navigations_ == hide)
    return;
  hide_future_navigations_ = hide;
  PropagateHideFutureNavigationsToHistoryTabHelper();
}

void TabAndroid::SetDevToolsAgentHost(
    scoped_refptr<content::DevToolsAgentHost> host) {
  devtools_host_ = std::move(host);
}

void TabAndroid::PropagateHideFutureNavigationsToHistoryTabHelper() {
  if (!web_contents())
    return;
  auto* history_tab_helper = HistoryTabHelper::FromWebContents(web_contents());
  if (history_tab_helper)
    history_tab_helper->set_hide_all_navigations(hide_future_navigations_);
}

base::android::ScopedJavaLocalRef<jobject> JNI_TabImpl_FromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  base::android::ScopedJavaLocalRef<jobject> jtab;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  TabAndroid* tab =
      web_contents ? TabAndroid::FromWebContents(web_contents) : nullptr;
  if (tab)
    jtab = tab->GetJavaObject();
  return jtab;
}

static void JNI_TabImpl_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  TRACE_EVENT0("native", "TabAndroid::Init");
  // This will automatically bind to the Java object and pass ownership there.
  new TabAndroid(env, obj);
}
