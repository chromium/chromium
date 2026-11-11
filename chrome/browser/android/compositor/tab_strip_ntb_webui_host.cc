// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_ntb_webui_host.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/tabstrip_ntb/tabstrip_ntb_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/tab_strip/tab_strip_service.h"
#include "components/browser_apis/tab_strip/tab_strip_service_impl.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/view_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NtbWebUiCoordinator_jni.h"

namespace android {

namespace {

// Maximum number of immediate recoveries before delaying retries.
constexpr uint32_t kMaxCrashRecoveryTimes = 3;
// Time without a crash after which `crash_count_` is reset.
constexpr base::TimeDelta kCrashRecoverResetInterval = base::Seconds(10);
// Delay before retrying recovery once `kMaxCrashRecoveryTimes` is exceeded.
constexpr base::TimeDelta kCrashRecoverRetryInterval = base::Minutes(1);

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

bool IsActivatingInputEvent(const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureTap:
      return true;
    case blink::WebInputEvent::Type::kMouseUp:
      return static_cast<const blink::WebMouseEvent&>(event).button ==
             blink::WebMouseEvent::Button::kLeft;
    case blink::WebInputEvent::Type::kKeyDown:
      return static_cast<const blink::WebKeyboardEvent&>(event)
                 .windows_key_code == ui::VKEY_RETURN;
    case blink::WebInputEvent::Type::kKeyUp:
      return static_cast<const blink::WebKeyboardEvent&>(event)
                 .windows_key_code == ui::VKEY_SPACE;
    default:
      return false;
  }
}

}  // namespace

TabStripNtbWebUiHost::TabStripNtbWebUiHost(
    Profile* profile,
    base::TimeTicks activity_creation_time,
    const base::android::JavaRef<jobject>& java_coordinator)
    : activity_creation_time_(activity_creation_time),
      java_coordinator_(java_coordinator) {
  TRACE_EVENT("ui", "TabStripNtbWebUiHost::TabStripNtbWebUiHost");
  CHECK(profile);
  profile_observation_.Observe(profile);
  const GURL url(chrome::kChromeUITabStripNtbURL);
  web_contents_ = CreateWebContents(profile, url);
  web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  web_contents_->SetDelegate(this);
  waap::PrewarmHelper::ConfigureWebUIContents(web_contents_.get(), profile,
                                              IDS_ACCNAME_NEWTAB);
  Observe(web_contents_.get());
  if (content::RenderFrameHost* main_frame =
          web_contents_->GetPrimaryMainFrame()) {
    input_event_observation_.Observe(main_frame->GetRenderWidgetHost());
  }

  LoadWebUi();
}

TabStripNtbWebUiHost::~TabStripNtbWebUiHost() {
  Reset();
}

void TabStripNtbWebUiHost::SetTabModel(TabModel* tab_model) {
  CHECK(tab_model);
  CHECK(!tab_model_);
  if (!web_contents_) {
    return;
  }
  CHECK_EQ(web_contents_->GetBrowserContext(), tab_model->GetProfile());
  tab_model_ = tab_model;
  TabModelList::AddObserver(this, profile_observation_.GetSource());
  service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
      tabs_api::AndroidTabStripApiInjector::CreateWithoutEventBridge(
          tab_model, TabModel::TabLaunchType::FROM_CHROME_UI));
  tabstrip_ntb::TabStripNtbUI::SetTabStripServiceForWebContents(
      web_contents_.get(), service_.get());
}

void TabStripNtbWebUiHost::OnProfileWillBeDestroyed(Profile* profile) {
  // IncognitoTabModelImpl notifies IncognitoProfileDestroyer::didBecomeEmpty()
  // before destroying the delegate TabModel (and firing OnTabModelRemoved).
  // Tear down the WebContents here during MaybeSendDestroyedNotification() so
  // its RenderProcessHost is unregistered before ProfileDestroyer checks for
  // live hosts.
  Reset();
}

void TabStripNtbWebUiHost::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model != tab_model_) {
    return;
  }
  // An incognito window destroys its TabModel as soon as its last tab closes
  // (even when another incognito window keeps the OTR Profile alive), before
  // the window itself is torn down. Tear down the WebContents and service now
  // so that the adapters' TabModel references do not dangle.
  Reset();
}

void TabStripNtbWebUiHost::LoadWebUi() {
  if (!web_contents_) {
    return;
  }
  content::NavigationController::LoadURLParams load_params(
      (GURL(chrome::kChromeUITabStripNtbURL)));
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

void TabStripNtbWebUiHost::Reset() {
  if (!web_contents_) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (tab_model_) {
    tab_model_ = nullptr;
    TabModelList::RemoveObserver(this, profile_observation_.GetSource());
  }
  profile_observation_.Reset();
  input_event_observation_.Reset();
  java_coordinator_.Reset();
  Observe(nullptr);
  web_contents_->SetDelegate(nullptr);
  // Destroy before |service_| so TabStripServiceUserData is torn down first.
  // Also detaches the scene layer, clears the Java WebContents/EventForwarder
  // pointers, and releases the Profile's RenderProcessHost.
  web_contents_.reset();
  // Destroy before |tab_model_| finishes destruction so the adapters'
  // TabModel* never dangles.
  service_.reset();
}

void TabStripNtbWebUiHost::Resize(int32_t width_px, int32_t height_px) {
  if (!web_contents_) {
    return;
  }
  ui::ViewAndroid* view = web_contents_->GetNativeView();
  view->OnPhysicalBackingSizeChanged(gfx::Size(width_px, height_px));
  view->OnSizeChanged(width_px, height_px);
}

void TabStripNtbWebUiHost::SetButtonVisible(bool visible) {
  if (!web_contents_) {
    return;
  }
  tabstrip_ntb::TabStripNtbUI::SetButtonVisibleForWebContents(
      web_contents_.get(), visible);
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

void TabStripNtbWebUiHost::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!new_host->IsInPrimaryMainFrame()) {
    return;
  }
  input_event_observation_.Reset();
  input_event_observation_.Observe(new_host->GetRenderWidgetHost());
}

void TabStripNtbWebUiHost::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // Reload on all termination statuses, including
  // TERMINATION_STATUS_NORMAL_TERMINATION, which Android reports when the OS
  // kills a renderer while Chrome is in the background or holds a waived
  // binding.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_crash_time_ >= kCrashRecoverResetInterval) {
    crash_count_ = 0;
  }
  last_crash_time_ = now;
  base::OnceCallback callback = base::BindOnce(&TabStripNtbWebUiHost::LoadWebUi,
                                               weak_ptr_factory_.GetWeakPtr());
  if (++crash_count_ <= kMaxCrashRecoveryTimes) {
    // PostTask avoids re-entering RenderProcessHostImpl::ProcessDied().
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(callback), kCrashRecoverRetryInterval);
  }
}

void TabStripNtbWebUiHost::OnFirstContentfulPaintInPrimaryMainFrame(
    base::TimeTicks presentation_time) {
  if (recorded_first_paint_ || activity_creation_time_.is_null()) {
    return;
  }
  recorded_first_paint_ = true;
  if (presentation_time >= activity_creation_time_) {
    TRACE_EVENT_INSTANT("ui", "TabStripNewTabButtonFirstPaint",
                        presentation_time);
    base::UmaHistogramMediumTimes(
        "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint",
        presentation_time - activity_creation_time_);
  }
}

bool TabStripNtbWebUiHost::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppress the default context menu for the WebUI new tab button.
  return true;
}

void TabStripNtbWebUiHost::OnInputEvent(const content::RenderWidgetHost& widget,
                                        const blink::WebInputEvent& event,
                                        input::InputEventSource source) {
  // Assumes the new tab button covers the entire WebUI surface, so any
  // activating input event reaching the WebContents targets the button.
  if (java_coordinator_ && IsActivatingInputEvent(event)) {
    Java_NtbWebUiCoordinator_onInputInteraction(
        base::android::AttachCurrentThread(), java_coordinator_,
        event.TimeStamp().ToUptimeMillis());
  }
}

content::WebContents* TabStripNtbWebUiHost::GetWebContents() {
  return web_contents_.get();
}

static jni_zero::JniUniquePtr<TabStripNtbWebUiHost>
JNI_NtbWebUiCoordinator_Init(const base::android::JavaRef<jobject>& caller,
                             Profile* profile,
                             int64_t activity_creation_uptime_ms) {
  base::TimeTicks activity_creation_time =
      activity_creation_uptime_ms > 0
          ? base::TimeTicks::FromUptimeMillis(activity_creation_uptime_ms)
          : base::TimeTicks();
  return jni_zero::MakeUnique<TabStripNtbWebUiHost>(
      profile, activity_creation_time, caller);
}

static void JNI_NtbWebUiCoordinator_SetTabModel(TabStripNtbWebUiHost* host,
                                                int32_t session_id) {
  SessionID id = SessionID::FromSerializedValue(session_id);
  CHECK(id.is_valid());
  TabModel* tab_model = TabModelList::FindTabModelWithWindowSessionId(id);
  CHECK(tab_model);
  host->SetTabModel(tab_model);
}

}  // namespace android

DEFINE_JNI(NtbWebUiCoordinator)
