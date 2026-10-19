// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_ntb_webui_host.h"

#include "base/check.h"
#include "base/location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/webui/tabstrip_ntb/tabstrip_ntb_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_apis/tab_strip/tab_strip_service.h"
#include "components/browser_apis/tab_strip/tab_strip_service_impl.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/view_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NtbWebUiCoordinator_jni.h"

namespace android {

namespace {

std::unique_ptr<content::WebContents> CreateWebContents(Profile* profile,
                                                        const GURL& url) {
  // Creating the WebContents with a SiteInstance for the WebUI URL (rather than
  // an unassigned one) lets top-chrome WebUIs share a renderer process across
  // windows.
  content::WebContents::CreateParams params(
      profile, content::SiteInstance::CreateForURL(profile, url));
  params.initially_hidden = false;
  return content::WebContents::Create(params);
}

}  // namespace

// TODO(crbug.com/498763314): Enable the event bridge when it supports
// lazily-restored Android tabs without WebContents. The NTB does not observe
// tab strip events.
TabStripNtbWebUiHost::TabStripNtbWebUiHost(TabModel* tab_model)
    : tab_model_(tab_model),
      service_(std::make_unique<tabs_api::TabStripServiceImpl>(
          std::make_unique<tabs_api::AndroidTabStripApiInjector>(
              tab_model,
              /*enable_event_bridge=*/false))) {
  TabModelList::AddObserver(this);
  const GURL url(chrome::kChromeUITabStripNtbURL);
  web_contents_ = CreateWebContents(tab_model->GetProfile(), url);
  web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  web_contents_->SetDelegate(this);
  // The page only paints the button; the tab strip behind must show through.
  web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  // Attached before navigating, so the WebUI can bind to the service during
  // construction.
  tabstrip_ntb::TabStripNtbUI::SetTabStripServiceForWebContents(
      web_contents_.get(), service_.get());
  Observe(web_contents_.get());

  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

TabStripNtbWebUiHost::~TabStripNtbWebUiHost() {
  Reset();
}

void TabStripNtbWebUiHost::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model != tab_model_) {
    return;
  }
  // An incognito window destroys its TabModel as soon as its last tab closes,
  // before the window is torn down. Tear down the WebContents and service now
  // so that the WebContents does not keep a RenderProcessHost for the
  // off-the-record Profile alive and block ProfileDestroyer.
  Reset();
}

void TabStripNtbWebUiHost::Reset() {
  if (!web_contents_) {
    return;
  }
  Observe(nullptr);
  web_contents_->SetDelegate(nullptr);
  // Destroy before |service_| so TabStripServiceUserData is torn down first.
  // Also detaches the scene layer, clears the Java WebContents/EventForwarder
  // pointers, and releases the Profile's RenderProcessHost.
  web_contents_.reset();
  // Destroy before |tab_model_| finishes destruction so the adapters'
  // TabModel* never dangles.
  service_.reset();
  tab_model_ = nullptr;
  TabModelList::RemoveObserver(this);
}

void TabStripNtbWebUiHost::Resize(int32_t width_px, int32_t height_px) {
  if (!web_contents_) {
    return;
  }
  ui::ViewAndroid* view = web_contents_->GetNativeView();
  view->OnPhysicalBackingSizeChanged(gfx::Size(width_px, height_px));
  view->OnSizeChanged(width_px, height_px);
}

scoped_refptr<cc::slim::Layer> TabStripNtbWebUiHost::GetLayer() {
  if (!web_contents_) {
    return nullptr;
  }
  return web_contents_->GetNativeView()->GetLayer();
}

void TabStripNtbWebUiHost::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (content::RenderWidgetHostView* view = render_frame_host->GetView()) {
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

bool TabStripNtbWebUiHost::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppress the default context menu for the WebUI new tab button.
  return true;
}

content::WebContents* TabStripNtbWebUiHost::GetWebContents() {
  return web_contents_.get();
}

static int64_t JNI_NtbWebUiCoordinator_Init(JNIEnv* env, int32_t session_id) {
  SessionID id = SessionID::FromSerializedValue(session_id);
  CHECK(id.is_valid());
  TabModel* tab_model = TabModelList::FindTabModelWithWindowSessionId(id);
  CHECK(tab_model);
  return reinterpret_cast<intptr_t>(new TabStripNtbWebUiHost(tab_model));
}

static content::WebContents* JNI_NtbWebUiCoordinator_GetWebContents(
    JNIEnv* env,
    int64_t host_ptr) {
  return reinterpret_cast<TabStripNtbWebUiHost*>(host_ptr)->GetWebContents();
}

static void JNI_NtbWebUiCoordinator_Resize(JNIEnv* env,
                                           int64_t host_ptr,
                                           int32_t width_px,
                                           int32_t height_px) {
  reinterpret_cast<TabStripNtbWebUiHost*>(host_ptr)->Resize(width_px,
                                                            height_px);
}

static void JNI_NtbWebUiCoordinator_Destroy(JNIEnv* env, int64_t host_ptr) {
  delete reinterpret_cast<TabStripNtbWebUiHost*>(host_ptr);
}

}  // namespace android

DEFINE_JNI(NtbWebUiCoordinator)
