// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "chrome/browser/android/background_tab_manager.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "chrome/browser/android/metrics/uma_utils.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/android/trusted_cdn.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/content_settings/popup_blocked_infobar_delegate.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/navigation_interception/navigation_params.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_request_body_android.h"
#include "jni/Tab_jni.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/escape.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using chrome::android::BackgroundTabManager;
using content::BrowserThread;
using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;
using navigation_interception::InterceptNavigationDelegate;
using navigation_interception::NavigationParams;

namespace {

GURL GetPublisherURLForTrustedCDN(
    content::NavigationHandle* navigation_handle) {
  if (!trusted_cdn::IsTrustedCDN(navigation_handle->GetURL()))
    return GURL();

  // Offline pages don't have headers when they are loaded.
  // TODO(bauerb): Consider storing the publisher URL on the offline page item.
  if (offline_pages::OfflinePageUtils::GetOfflinePageFromWebContents(
          navigation_handle->GetWebContents())) {
    return GURL();
  }

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (!headers) {
    // TODO(https://crbug.com/829323): In some cases other than offline pages
    // we don't have headers.
    LOG(WARNING) << "No headers for navigation to "
                 << navigation_handle->GetURL();
    return GURL();
  }

  std::string publisher_url;
  if (!headers->GetNormalizedHeader("x-amp-cache", &publisher_url))
    return GURL();

  return GURL(publisher_url);
}

}  // namespace

// This class is created and owned by the MediaDownloadInProductHelpManager.
class TabAndroid::MediaDownloadInProductHelp
    : public blink::mojom::MediaDownloadInProductHelp {
 public:
  MediaDownloadInProductHelp(
      content::RenderFrameHost* render_frame_host,
      TabAndroid* tab,
      blink::mojom::MediaDownloadInProductHelpRequest request)
      : render_frame_host_(render_frame_host),
        tab_(tab),
        binding_(this, std::move(request)) {
    DCHECK(render_frame_host_);
    DCHECK(tab_);

    binding_.set_connection_error_handler(
        base::BindOnce(&TabAndroid::OnMediaDownloadInProductHelpConnectionError,
                       base::Unretained(tab_)));
  }
  ~MediaDownloadInProductHelp() override = default;

  // blink::mojom::MediaPromoUI implementation.
  void ShowInProductHelpWidget(const gfx::Rect& rect) override {
    tab_->ShowMediaDownloadInProductHelp(rect);
  }

  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }

 private:
  // The |manager_| and |render_frame_host_| outlive this class.
  content::RenderFrameHost* const render_frame_host_;
  TabAndroid* tab_;
  mojo::Binding<blink::mojom::MediaDownloadInProductHelp> binding_;
};

TabAndroid* TabAndroid::FromWebContents(
  const content::WebContents* web_contents) {
  const CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(
      web_contents);
  if (!core_tab_helper)
    return NULL;

  CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
  if (!core_delegate)
    return NULL;

  return static_cast<TabAndroid*>(core_delegate);
}

TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, const JavaRef<jobject>& obj) {
  return reinterpret_cast<TabAndroid*>(Java_Tab_getNativePtr(env, obj));
}

void TabAndroid::AttachTabHelpers(content::WebContents* web_contents) {
  DCHECK(web_contents);

  TabHelpers::AttachTabHelpers(web_contents);
}

TabAndroid::TabAndroid(JNIEnv* env, const JavaRef<jobject>& obj)
    : weak_java_tab_(env, obj),
      session_window_id_(SessionID::InvalidValue()),
      content_layer_(cc::Layer::Create()),
      tab_content_manager_(NULL),
      synced_tab_delegate_(new browser_sync::SyncedTabDelegateAndroid(this)),
      picture_in_picture_enabled_(false),
      embedded_media_experience_enabled_(false),
      weak_factory_(this) {
  Java_Tab_setNativePtr(env, obj, reinterpret_cast<intptr_t>(this));

  frame_interfaces_.AddInterface(base::Bind(
      &TabAndroid::CreateInProductHelpService, weak_factory_.GetWeakPtr()));
}

TabAndroid::~TabAndroid() {
  GetContentLayer()->RemoveAllChildren();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_clearNativePtr(env, weak_java_tab_.get(env));
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
  return Java_Tab_getId(env, weak_java_tab_.get(env));
}

base::string16 TabAndroid::GetTitle() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF16(
      Java_Tab_getTitle(env, weak_java_tab_.get(env)));
}

bool TabAndroid::IsNativePage() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_isNativePage(env, weak_java_tab_.get(env));
}

GURL TabAndroid::GetURL() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return GURL(base::android::ConvertJavaStringToUTF8(
      Java_Tab_getUrl(env, weak_java_tab_.get(env))));
}

bool TabAndroid::IsUserInteractable() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_isUserInteractable(env, weak_java_tab_.get(env));
}

bool TabAndroid::LoadIfNeeded() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_loadIfNeeded(env, weak_java_tab_.get(env));
}

Profile* TabAndroid::GetProfile() const {
  if (!web_contents())
    return NULL;

  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

sync_sessions::SyncedTabDelegate* TabAndroid::GetSyncedTabDelegate() const {
  return synced_tab_delegate_.get();
}

void TabAndroid::DeleteFrozenNavigationEntries(
    const WebContentsState::DeletionPredicate& predicate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_deleteNavigationEntriesFromFrozenState(
      env, weak_java_tab_.get(env), reinterpret_cast<intptr_t>(&predicate));
}

void TabAndroid::SetWindowSessionID(SessionID window_id) {
  session_window_id_ = window_id;

  if (!web_contents())
    return;

  SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents());
  session_tab_helper->SetWindowID(session_window_id_);
}

void TabAndroid::HandlePopupNavigation(NavigateParams* params) {
  DCHECK(params->source_contents == web_contents());
  DCHECK(!params->contents_to_insert);
  DCHECK(!params->switch_to_singleton_tab);

  WindowOpenDisposition disposition = params->disposition;
  const GURL& url = params->url;

  if (disposition == WindowOpenDisposition::NEW_POPUP ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_WINDOW ||
      disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> jobj = weak_java_tab_.get(env);
    ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(env, url.spec()));
    ScopedJavaLocalRef<jstring> jheaders(
        ConvertUTF8ToJavaString(env, params->extra_headers));
    ScopedJavaLocalRef<jobject> jpost_data;
    if (params->uses_post && params->post_data) {
      jpost_data = content::ConvertResourceRequestBodyToJavaObject(
          env, params->post_data);
    }
    Java_Tab_openNewTab(
        env, jobj, jurl, jheaders, jpost_data, static_cast<int>(disposition),
        params->created_with_opener, params->is_renderer_initiated);
  } else {
    NOTIMPLEMENTED();
  }
}

bool TabAndroid::HasPrerenderedUrl(GURL gurl) {
  prerender::PrerenderManager* prerender_manager = GetPrerenderManager();
  if (!prerender_manager)
    return false;

  std::vector<content::WebContents*> contents =
      prerender_manager->GetAllPrerenderingContents();
  prerender::PrerenderContents* prerender_contents;
  for (size_t i = 0; i < contents.size(); ++i) {
    prerender_contents = prerender_manager->
        GetPrerenderContents(contents.at(i));
    if (prerender_contents->prerender_url() == gurl &&
        prerender_contents->has_finished_loading()) {
      return true;
    }
  }
  return false;
}

void TabAndroid::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      TabSpecificContentSettings* settings =
          TabSpecificContentSettings::FromWebContents(web_contents());
      if (!settings->IsBlockageIndicated(CONTENT_SETTINGS_TYPE_POPUPS)) {
        // TODO(dfalcantara): Create an InfoBarDelegate to keep the
        // PopupBlockedInfoBar logic native-side instead of straddling the JNI
        // boundary.
        int num_popups = 0;
        PopupBlockerTabHelper* popup_blocker_helper =
            PopupBlockerTabHelper::FromWebContents(web_contents());
        if (popup_blocker_helper)
          num_popups = popup_blocker_helper->GetBlockedPopupsCount();

        if (num_popups > 0)
          PopupBlockedInfoBarDelegate::Create(web_contents(), num_popups);

        settings->SetBlockageHasBeenIndicated(CONTENT_SETTINGS_TYPE_POPUPS);
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

void TabAndroid::OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                                  NotificationIconType notification_icon_type,
                                  const GURL& icon_url,
                                  bool icon_url_changed,
                                  const gfx::Image& image) {
  if (notification_icon_type != NON_TOUCH_LARGEST &&
      notification_icon_type != TOUCH_LARGEST) {
    return;
  }

  SkBitmap favicon = image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  if (favicon.empty())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_onFaviconAvailable(env, weak_java_tab_.get(env),
                              gfx::ConvertToJavaBitmap(&favicon));
}

bool TabAndroid::IsCurrentlyACustomTab() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Tab_isCurrentlyACustomTab(env, weak_java_tab_.get(env));
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
    const JavaParamRef<jobject>& jcontext_menu_populator) {
  web_contents_.reset(content::WebContents::FromJavaWebContents(jweb_contents));
  DCHECK(web_contents_.get());

  AttachTabHelpers(web_contents_.get());
  WebContentsObserver::Observe(web_contents_.get());

  SetWindowSessionID(session_window_id_);

  ContextMenuHelper::FromWebContents(web_contents())->SetPopulator(
      jcontext_menu_populator);
  ViewAndroidHelper::FromWebContents(web_contents())->
      SetViewAndroid(web_contents()->GetNativeView());
  CoreTabHelper::FromWebContents(web_contents())->set_delegate(this);
  web_contents_delegate_ =
      std::make_unique<android::TabWebContentsDelegateAndroid>(
          env, jweb_contents_delegate);
  web_contents_delegate_->LoadProgressChanged(web_contents(), 0);
  web_contents()->SetDelegate(web_contents_delegate_.get());

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());

  if (favicon_driver)
    favicon_driver->AddObserver(this);

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
    const JavaParamRef<jobject>& jcontext_menu_populator) {
  ContextMenuHelper::FromWebContents(web_contents())->SetPopulator(
      jcontext_menu_populator);
  web_contents_delegate_ =
      std::make_unique<android::TabWebContentsDelegateAndroid>(
          env, jweb_contents_delegate);
  web_contents()->SetDelegate(web_contents_delegate_.get());
}

void TabAndroid::DestroyWebContents(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jboolean delete_native) {
  DCHECK(web_contents());

  if (web_contents()->GetNativeView())
    web_contents()->GetNativeView()->GetLayer()->RemoveFromParent();

  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  WebContentsObserver::Observe(nullptr);

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());

  if (favicon_driver)
    favicon_driver->RemoveObserver(this);

  web_contents()->SetDelegate(NULL);

  if (delete_native) {
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
  } else {
    // Remove the link from the native WebContents to |this|, since the
    // lifetimes of the two objects are no longer intertwined.
    CoreTabHelper::FromWebContents(web_contents())->set_delegate(nullptr);
    // Release the WebContents so it does not get deleted by the scoped_ptr.
    ignore_result(web_contents_.release());
  }
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

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetProfileAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  Profile* profile = GetProfile();
  if (!profile)
    return base::android::ScopedJavaLocalRef<jobject>();
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (!profile_android)
    return base::android::ScopedJavaLocalRef<jobject>();

  return profile_android->GetJavaObject();
}

TabAndroid::TabLoadStatus TabAndroid::LoadUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& j_extra_headers,
    const JavaParamRef<jobject>& j_post_data,
    jint page_transition,
    const JavaParamRef<jstring>& j_referrer_url,
    jint referrer_policy,
    jboolean is_renderer_initiated,
    jboolean should_replace_current_entry,
    jboolean has_user_gesture,
    jboolean should_clear_history_list,
    jlong input_start_timestamp) {
  if (!web_contents())
    return PAGE_LOAD_FAILED;

  if (url.is_null())
    return PAGE_LOAD_FAILED;

  GURL gurl(base::android::ConvertJavaStringToUTF8(env, url));
  if (gurl.is_empty())
    return PAGE_LOAD_FAILED;

  // If the page was prerendered, use it.
  // Note in incognito mode, we don't have a PrerenderManager.

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(GetProfile());
  if (prerender_manager) {
    bool prefetched_page_loaded = HasPrerenderedUrl(gurl);
    // Getting the load status before MaybeUsePrerenderedPage() b/c it resets.
    prerender::PrerenderManager::Params params(
        /*uses_post=*/false, /*extra_headers=*/std::string(),
        /*should_replace_current_entry=*/false, web_contents());
    if (prerender_manager->MaybeUsePrerenderedPage(gurl, &params)) {
      return prefetched_page_loaded ?
          FULL_PRERENDERED_PAGE_LOAD : PARTIAL_PRERENDERED_PAGE_LOAD;
    }
  }

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
          static_cast<network::mojom::ReferrerPolicy>(referrer_policy));
    }
    load_params.is_renderer_initiated = is_renderer_initiated;
    load_params.should_replace_current_entry = should_replace_current_entry;
    load_params.has_user_gesture = has_user_gesture;
    load_params.should_clear_history_list = should_clear_history_list;
    if (input_start_timestamp != 0) {
      load_params.input_start =
          base::TimeTicks::FromUptimeMillis(input_start_timestamp);
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

bool TabAndroid::Print(JNIEnv* env,
                       const JavaParamRef<jobject>& obj,
                       jint render_process_id,
                       jint render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents())
    return false;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  if (!rfh)
    rfh = printing::GetFrameToPrint(web_contents());

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);

  printing::PrintViewManagerBasic::CreateForWebContents(contents);
  printing::PrintViewManagerBasic* print_view_manager =
      printing::PrintViewManagerBasic::FromWebContents(contents);
  if (!print_view_manager)
    return false;

  if (!print_view_manager->PrintNow(rfh))
    return false;

  return true;
}

void TabAndroid::SetPendingPrint(int render_process_id, int render_frame_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_setPendingPrint(env, weak_java_tab_.get(env), render_process_id,
                           render_frame_id);
}

ScopedJavaLocalRef<jobject> TabAndroid::GetFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ScopedJavaLocalRef<jobject> bitmap;
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_.get());

  if (!favicon_driver)
    return bitmap;

  // Always return the default favicon in Android.
  SkBitmap favicon = favicon_driver->GetFavicon().AsBitmap();
  if (!favicon.empty()) {
    const float device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
    int target_size_dip = device_scale_factor * gfx::kFaviconSize;
    if (favicon.width() != target_size_dip ||
        favicon.height() != target_size_dip) {
      favicon =
          skia::ImageOperations::Resize(favicon,
                                        skia::ImageOperations::RESIZE_BEST,
                                        target_size_dip,
                                        target_size_dip);
    }

    bitmap = gfx::ConvertToJavaBitmap(&favicon);
  }
  return bitmap;
}

prerender::PrerenderManager* TabAndroid::GetPrerenderManager() const {
  Profile* profile = GetProfile();
  if (!profile)
    return NULL;
  return prerender::PrerenderManagerFactory::GetForBrowserContext(profile);
}

// static
void TabAndroid::CreateHistoricalTabFromContents(WebContents* web_contents) {
  DCHECK(web_contents);

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service)
    return;

  // Exclude internal pages from being marked as recent when they are closed.
  const GURL& tab_url = web_contents->GetURL();
  if (tab_url.SchemeIs(content::kChromeUIScheme) ||
      tab_url.SchemeIs(chrome::kChromeNativeScheme) ||
      tab_url.SchemeIs(url::kAboutScheme)) {
    return;
  }

  // TODO(jcivelli): is the index important?
  service->CreateHistoricalTab(
      sessions::ContentLiveTab::GetForWebContents(web_contents), -1);
}

void TabAndroid::CreateHistoricalTab(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  TabAndroid::CreateHistoricalTabFromContents(web_contents());
}

void TabAndroid::UpdateBrowserControlsState(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jint constraints,
                                            jint current,
                                            jboolean animate) {
  content::BrowserControlsState constraints_state =
      static_cast<content::BrowserControlsState>(constraints);
  content::BrowserControlsState current_state =
      static_cast<content::BrowserControlsState>(current);

  chrome::mojom::ChromeRenderFrameAssociatedPtr renderer;
  web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &renderer);
  renderer->UpdateBrowserControlsState(constraints_state, current_state,
                                       animate);

  if (web_contents()->ShowingInterstitialPage()) {
    chrome::mojom::ChromeRenderFrameAssociatedPtr interstitial_renderer;
    web_contents()
        ->GetInterstitialPage()
        ->GetMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&interstitial_renderer);
    interstitial_renderer->UpdateBrowserControlsState(constraints_state,
                                                      current_state, animate);
  }
}

void TabAndroid::LoadOriginalImage(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  content::RenderFrameHost* render_frame_host =
      web_contents()->GetFocusedFrame();
  chrome::mojom::ChromeRenderFrameAssociatedPtr renderer;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&renderer);
  renderer->RequestReloadImageForContextNode();
}

jlong TabAndroid::GetBookmarkId(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                jboolean only_editable) {
  GURL url = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
      web_contents()->GetURL());
  Profile* profile = GetProfile();

  // Get all the nodes for |url| and sort them by date added.
  std::vector<const bookmarks::BookmarkNode*> nodes;
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile);
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);

  model->GetNodesByURL(url, &nodes);
  std::sort(nodes.begin(), nodes.end(), &bookmarks::MoreRecentlyAdded);

  // Return the first node matching the search criteria.
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (only_editable && !managed->CanBeEditedByUser(nodes[i]))
      continue;
    return nodes[i]->id();
  }

  return -1;
}

bool TabAndroid::HasPrerenderedUrl(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jstring>& url) {
  GURL gurl(base::android::ConvertJavaStringToUTF8(env, url));
  return HasPrerenderedUrl(gurl);
}

void TabAndroid::EnableEmbeddedMediaExperience(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean enabled) {
  embedded_media_experience_enabled_ = enabled;

  if (!web_contents() || !web_contents()->GetRenderViewHost())
    return;

  web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
}

bool TabAndroid::ShouldEnableEmbeddedMediaExperience() const {
  return embedded_media_experience_enabled_;
}

void TabAndroid::SetPictureInPictureEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean enabled) {
  picture_in_picture_enabled_ = enabled;

  if (!web_contents() || !web_contents()->GetRenderViewHost())
    return;

  web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
}

bool TabAndroid::IsPictureInPictureEnabled() const {
  return picture_in_picture_enabled_;
}

void TabAndroid::AttachDetachedTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  BackgroundTabManager* background_tab_manager =
      BackgroundTabManager::GetInstance();
  if (background_tab_manager->IsBackgroundTab(web_contents())) {
    Profile* profile = background_tab_manager->GetProfile();
    background_tab_manager->CommitHistory(HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::IMPLICIT_ACCESS));
    background_tab_manager->UnregisterBackgroundTab();
  }
}

namespace {

class ChromeInterceptNavigationDelegate : public InterceptNavigationDelegate {
 public:
  ChromeInterceptNavigationDelegate(JNIEnv* env, jobject jdelegate)
      : InterceptNavigationDelegate(env, jdelegate) {}

  bool ShouldIgnoreNavigation(
      const NavigationParams& navigation_params) override {
    NavigationParams chrome_navigation_params(navigation_params);
    chrome_navigation_params.url() =
        GURL(net::EscapeExternalHandlerValue(navigation_params.url().spec()));
    return InterceptNavigationDelegate::ShouldIgnoreNavigation(
        chrome_navigation_params);
  }
};

}  // namespace

void TabAndroid::SetInterceptNavigationDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InterceptNavigationDelegate::Associate(
      web_contents(),
      std::make_unique<ChromeInterceptNavigationDelegate>(env, delegate));
}

void TabAndroid::SetWebappManifestScope(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jstring>& scope) {
  webapp_manifest_scope_ = GURL(base::android::ConvertJavaStringToUTF8(scope));

  if (!web_contents() || !web_contents()->GetRenderViewHost())
    return;

  web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
}

void TabAndroid::AttachToTabContentManager(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jtab_content_manager) {
  android::TabContentManager* tab_content_manager =
      android::TabContentManager::FromJavaObject(jtab_content_manager);
  if (tab_content_manager == tab_content_manager_)
    return;

  if (tab_content_manager_)
    tab_content_manager_->DetachLiveLayer(GetAndroidId(), GetContentLayer());
  tab_content_manager_ = tab_content_manager;
  if (tab_content_manager_)
    tab_content_manager_->AttachLiveLayer(GetAndroidId(), GetContentLayer());
}

void TabAndroid::ClearThumbnailPlaceholder(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  if (tab_content_manager_)
    tab_content_manager_->NativeRemoveTabThumbnail(GetAndroidId());
}

jint TabAndroid::GetCurrentRenderProcessId(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  DCHECK(host);
  content::RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->IsInitializedAndNotDead())
    return render_process->GetProcess().Handle();
  return 0;
}

void TabAndroid::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  frame_interfaces_.TryBindInterface(interface_name, interface_pipe,
                                     render_frame_host);
}

void TabAndroid::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (media_in_product_help_ &&
      media_in_product_help_->render_frame_host() == render_frame_host) {
    DismissMediaDownloadInProductHelp();
  }
}

void TabAndroid::NavigationEntryChanged(
    const content::EntryChangedDetails& change_details) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_onNavEntryChanged(env, weak_java_tab_.get(env));
}

void TabAndroid::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Skip subframe, same-document, or non-committed navigations (downloads or
  // 204/205 responses).
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  GURL publisher_url = GetPublisherURLForTrustedCDN(navigation_handle);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_publisher_url;
  if (publisher_url.is_valid())
    j_publisher_url = ConvertUTF8ToJavaString(env, publisher_url.spec());

  Java_Tab_setTrustedCdnPublisherUrl(env, weak_java_tab_.get(env),
                                     j_publisher_url);
}

bool TabAndroid::AreRendererInputEventsIgnored(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  content::RenderProcessHost* render_process_host =
      web_contents()->GetMainFrame()->GetProcess();
  return render_process_host->IgnoreInputEvents();
}

void TabAndroid::ShowMediaDownloadInProductHelp(
    const gfx::Rect& rect_in_frame) {
  DCHECK(web_contents_);

  // We need to account for the browser controls offset to get the location for
  // the widget in the view.
  gfx::NativeView view = web_contents_->GetNativeView();
  gfx::Rect rect_in_view(rect_in_frame.x(),
                         rect_in_frame.y() + view->content_offset(),
                         rect_in_frame.width(), rect_in_frame.height());
  gfx::Rect scaled_rect_on_screen = gfx::ScaleToEnclosingRectSafe(
      rect_in_view, ui::GetScaleFactorForNativeView(view));

  // We also need to account for the offset of the viewport location on screen.
  scaled_rect_on_screen.set_origin(
      scaled_rect_on_screen.origin() +
      view->GetLocationOfContainerViewInWindow().OffsetFromOrigin());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_showMediaDownloadInProductHelp(
      env, weak_java_tab_.get(env), scaled_rect_on_screen.x(),
      scaled_rect_on_screen.y(), scaled_rect_on_screen.width(),
      scaled_rect_on_screen.height());
}

void TabAndroid::DismissMediaDownloadInProductHelp() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Tab_hideMediaDownloadInProductHelp(env, weak_java_tab_.get(env));
}

void TabAndroid::MediaDownloadInProductHelpDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(media_in_product_help_);
  media_in_product_help_.reset();
}

void TabAndroid::CreateInProductHelpService(
    blink::mojom::MediaDownloadInProductHelpRequest request,
    content::RenderFrameHost* render_frame_host) {
  // If we are showing the UI already, ignore the request.
  if (media_in_product_help_)
    return;

  media_in_product_help_ = std::make_unique<MediaDownloadInProductHelp>(
      render_frame_host, this, std::move(request));
}

void TabAndroid::OnMediaDownloadInProductHelpConnectionError() {
  DCHECK(media_in_product_help_);
  DismissMediaDownloadInProductHelp();
}

scoped_refptr<content::DevToolsAgentHost> TabAndroid::GetDevToolsAgentHost() {
  return devtools_host_;
}

void TabAndroid::SetDevToolsAgentHost(
    scoped_refptr<content::DevToolsAgentHost> host) {
  devtools_host_ = std::move(host);
}

static void JNI_Tab_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  TRACE_EVENT0("native", "TabAndroid::Init");
  // This will automatically bind to the Java object and pass ownership there.
  new TabAndroid(env, obj);
}
