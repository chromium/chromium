// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_contents.h"

#include <stddef.h>

#include <functional>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/prerender_types.h"
#include "chrome/common/prerender_util.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"
#include "net/http/http_response_headers.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

namespace prerender {

namespace {

void ResumeThrottles(
    std::vector<base::WeakPtr<PrerenderResourceThrottle>> throttles,
    std::vector<base::WeakPtr<PrerenderResourceThrottle>> idle_resources) {
  for (auto resource : idle_resources) {
    if (resource)
      resource->ResetResourcePriority();
  }
  for (size_t i = 0; i < throttles.size(); i++) {
    if (throttles[i])
      throttles[i]->ResumeHandler();
  }
}

}  // namespace

class PrerenderContentsFactoryImpl : public PrerenderContents::Factory {
 public:
  PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin) override {
    return new PrerenderContents(prerender_manager, profile, url, referrer,
                                 origin);
  }
};

// WebContentsDelegateImpl -----------------------------------------------------

class PrerenderContents::WebContentsDelegateImpl
    : public content::WebContentsDelegate {
 public:
  explicit WebContentsDelegateImpl(PrerenderContents* prerender_contents)
      : prerender_contents_(prerender_contents) {
  }

  // content::WebContentsDelegate implementation:
  WebContents* OpenURLFromTab(WebContents* source,
                              const OpenURLParams& params) override {
    // |OpenURLFromTab| is typically called when a frame performs a navigation
    // that requires the browser to perform the transition instead of WebKit.
    // Examples include client redirects to hosted app URLs.
    // TODO(cbentzel): Consider supporting this for CURRENT_TAB dispositions, if
    // it is a common case during prerenders.
    prerender_contents_->Destroy(FINAL_STATUS_OPEN_URL);
    return NULL;
  }

  bool ShouldTransferNavigation(bool is_main_frame_navigation) override {
    // Cancel the prerender if the navigation attempts to transfer to a
    // different process.  Examples include server redirects to privileged pages
    // or cross-site subframe navigations in --site-per-process.
    prerender_contents_->Destroy(FINAL_STATUS_OPEN_URL);
    return false;
  }

  void CloseContents(content::WebContents* contents) override {
    prerender_contents_->Destroy(FINAL_STATUS_CLOSED);
  }

  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) override {
    prerender_contents_->Destroy(FINAL_STATUS_DOWNLOAD);
    // Cancel the download.
    callback.Run(false);
  }

  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      SessionStorageNamespace* session_storage_namespace) override {
    // Since we don't want to permit child windows that would have a
    // window.opener property, terminate prerendering.
    prerender_contents_->Destroy(FINAL_STATUS_CREATE_NEW_WINDOW);
    // Cancel the popup.
    return false;
  }

  bool OnGoToEntryOffset(int offset) override {
    // This isn't allowed because the history merge operation
    // does not work if there are renderer issued challenges.
    // TODO(cbentzel): Cancel in this case? May not need to do
    // since render-issued offset navigations are not guaranteed,
    // but indicates that the page cares about the history.
    return false;
  }

  bool ShouldSuppressDialogs(WebContents* source) override {
    // We still want to show the user the message when they navigate to this
    // page, so cancel this prerender.
    prerender_contents_->Destroy(FINAL_STATUS_JAVASCRIPT_ALERT);
    // Always suppress JavaScript messages if they're triggered by a page being
    // prerendered.
    return true;
  }

  void RegisterProtocolHandler(WebContents* web_contents,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override {
    // TODO(mmenke): Consider supporting this if it is a common case during
    // prerenders.
    prerender_contents_->Destroy(FINAL_STATUS_REGISTER_PROTOCOL_HANDLER);
  }

  gfx::Size GetSizeForNewRenderView(WebContents* web_contents) const override {
    // Have to set the size of the RenderView on initialization to be sure it is
    // set before the RenderView is hidden on all platforms (esp. Android).
    return prerender_contents_->bounds_.size();
  }

 private:
  PrerenderContents* prerender_contents_;
};

PrerenderContents::Observer::~Observer() {}

PrerenderContents::PrerenderContents(PrerenderManager* prerender_manager,
                                     Profile* profile,
                                     const GURL& url,
                                     const content::Referrer& referrer,
                                     Origin origin)
    : prerender_mode_(FULL_PRERENDER),
      prerendering_has_started_(false),
      prerender_canceler_binding_(this),
      prerender_manager_(prerender_manager),
      prerender_url_(url),
      referrer_(referrer),
      profile_(profile),
      has_finished_loading_(false),
      final_status_(FINAL_STATUS_MAX),
      prerendering_has_been_cancelled_(false),
      process_pid_(base::kNullProcessId),
      child_id_(-1),
      route_id_(-1),
      origin_(origin),
      network_bytes_(0),
      weak_factory_(this) {
  DCHECK(prerender_manager);
  registry_.AddInterface(base::Bind(
      &PrerenderContents::OnPrerenderCancelerRequest, base::Unretained(this)));
}

bool PrerenderContents::Init() {
  return AddAliasURL(prerender_url_);
}

void PrerenderContents::SetPrerenderMode(PrerenderMode mode) {
  DCHECK(!prerendering_has_started_);
  prerender_mode_ = mode;
}

// static
PrerenderContents::Factory* PrerenderContents::CreateFactory() {
  return new PrerenderContentsFactoryImpl();
}

// static
PrerenderContents* PrerenderContents::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!prerender_manager)
    return NULL;
  return prerender_manager->GetPrerenderContents(web_contents);
}

void PrerenderContents::StartPrerendering(
    const gfx::Rect& bounds,
    SessionStorageNamespace* session_storage_namespace) {
  DCHECK(profile_);
  DCHECK(!bounds.IsEmpty());
  DCHECK(!prerendering_has_started_);
  DCHECK(!prerender_contents_);
  DCHECK_EQ(1U, alias_urls_.size());

  session_storage_namespace_id_ = session_storage_namespace->id();
  bounds_ = bounds;

  DCHECK(load_start_time_.is_null());
  load_start_time_ = base::TimeTicks::Now();

  prerendering_has_started_ = true;

  prerender_contents_ = CreateWebContents(session_storage_namespace);
  TabHelpers::AttachTabHelpers(prerender_contents_.get());
  content::WebContentsObserver::Observe(prerender_contents_.get());

  // Tag the prerender contents with the task manager specific prerender tag, so
  // that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForPrerenderContents(
      prerender_contents_.get());

  web_contents_delegate_.reset(new WebContentsDelegateImpl(this));
  prerender_contents_.get()->SetDelegate(web_contents_delegate_.get());
  // Set the size of the prerender WebContents.
  ResizeWebContents(prerender_contents_.get(), bounds_);

  // TODO(davidben): This logic assumes each prerender has at most one
  // route. https://crbug.com/440544
  child_id_ = GetRenderViewHost()->GetProcess()->GetID();
  route_id_ = GetRenderViewHost()->GetRoutingID();

  // TODO(davidben): This logic assumes each prerender has at most one
  // process. https://crbug.com/440544
  prerender_manager()->AddPrerenderProcessHost(
      GetRenderViewHost()->GetProcess());

  NotifyPrerenderStart();

  // Close ourselves when the application is shutting down.
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  // Register to inform new RenderViews that we're prerendering.
  notification_registrar_.Add(
      this, content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
      content::Source<WebContents>(prerender_contents_.get()));

  // Transfer over the user agent override.
  prerender_contents_.get()->SetUserAgentOverride(
      prerender_manager_->config().user_agent_override, false);

  content::NavigationController::LoadURLParams load_url_params(
      prerender_url_);
  load_url_params.referrer = referrer_;
  load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
  if (origin_ == ORIGIN_OMNIBOX) {
    load_url_params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED |
        ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  }
  load_url_params.override_user_agent =
      prerender_manager_->config().is_overriding_user_agent ?
      content::NavigationController::UA_OVERRIDE_TRUE :
      content::NavigationController::UA_OVERRIDE_FALSE;
  prerender_contents_.get()->GetController().LoadURLWithParams(load_url_params);
}

bool PrerenderContents::GetChildId(int* child_id) const {
  CHECK(child_id);
  DCHECK_GE(child_id_, -1);
  *child_id = child_id_;
  return child_id_ != -1;
}

bool PrerenderContents::GetRouteId(int* route_id) const {
  CHECK(route_id);
  DCHECK_GE(route_id_, -1);
  *route_id = route_id_;
  return route_id_ != -1;
}

void PrerenderContents::SetFinalStatus(FinalStatus final_status) {
  DCHECK_GE(final_status, FINAL_STATUS_USED);
  DCHECK_LT(final_status, FINAL_STATUS_MAX);

  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);

  final_status_ = final_status;
}

PrerenderContents::~PrerenderContents() {
  DCHECK_NE(FINAL_STATUS_MAX, final_status());
  DCHECK(
      prerendering_has_been_cancelled() || final_status() == FINAL_STATUS_USED);
  DCHECK_NE(ORIGIN_MAX, origin());

  prerender_manager_->RecordFinalStatus(origin(), final_status());
  prerender_manager_->RecordNetworkBytesConsumed(origin(), network_bytes_);

  // Broadcast the removal of aliases.
  for (content::RenderProcessHost::iterator host_iterator =
           content::RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd();
       host_iterator.Advance()) {
    content::RenderProcessHost* host = host_iterator.GetCurrentValue();
    IPC::ChannelProxy* channel = host->GetChannel();
    // |channel| might be NULL in tests.
    if (channel) {
      chrome::mojom::PrerenderDispatcherAssociatedPtr prerender_dispatcher;
      channel->GetRemoteAssociatedInterface(&prerender_dispatcher);
      prerender_dispatcher->PrerenderRemoveAliases(alias_urls_);
    }
  }

  if (!prerender_contents_)
    return;

  // If we still have a WebContents, clean up anything we need to and then
  // destroy it.
  std::unique_ptr<WebContents> contents = ReleasePrerenderContents();
}

void PrerenderContents::AddObserver(Observer* observer) {
  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);
  observer_list_.AddObserver(observer);
}

void PrerenderContents::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PrerenderContents::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    // TODO(davidben): Try to remove this in favor of relying on
    // FINAL_STATUS_PROFILE_DESTROYED.
    case chrome::NOTIFICATION_APP_TERMINATING:
      Destroy(FINAL_STATUS_APP_TERMINATING);
      return;

    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
      if (prerender_contents_.get()) {
        DCHECK_EQ(content::Source<WebContents>(source).ptr(),
                  prerender_contents_.get());

        content::Details<RenderViewHost> new_render_view_host(details);
        OnRenderViewHostCreated(new_render_view_host.ptr());

        // Make sure the size of the RenderViewHost has been passed to the new
        // RenderView.  Otherwise, the size may not be sent until the
        // RenderViewReady event makes it from the render process to the UI
        // thread of the browser process.  When the RenderView receives its
        // size, is also sets itself to be visible, which would then break the
        // visibility API.
        new_render_view_host->GetWidget()->SynchronizeVisualProperties();
        prerender_contents_->WasHidden();
      }
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void PrerenderContents::OnRenderViewHostCreated(
    RenderViewHost* new_render_view_host) {
}

std::unique_ptr<WebContents> PrerenderContents::CreateWebContents(
    SessionStorageNamespace* session_storage_namespace) {
  // TODO(ajwong): Remove the temporary map once prerendering is aware of
  // multiple session storage namespaces per tab.
  content::SessionStorageNamespaceMap session_storage_namespace_map;
  session_storage_namespace_map[std::string()] = session_storage_namespace;
  return WebContents::CreateWithSessionStorage(
      WebContents::CreateParams(profile_), session_storage_namespace_map);
}

void PrerenderContents::NotifyPrerenderStart() {
  DCHECK_EQ(FINAL_STATUS_MAX, final_status_);
  for (Observer& observer : observer_list_)
    observer.OnPrerenderStart(this);
}

void PrerenderContents::NotifyPrerenderStopLoading() {
  for (Observer& observer : observer_list_)
    observer.OnPrerenderStopLoading(this);
}

void PrerenderContents::NotifyPrerenderDomContentLoaded() {
  for (Observer& observer : observer_list_)
    observer.OnPrerenderDomContentLoaded(this);
}

void PrerenderContents::NotifyPrerenderStop() {
  DCHECK_NE(FINAL_STATUS_MAX, final_status_);
  for (Observer& observer : observer_list_)
    observer.OnPrerenderStop(this);
  observer_list_.Clear();
}

bool PrerenderContents::CheckURL(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return false;
  }
  if (prerender_manager_->HasRecentlyBeenNavigatedTo(origin(), url)) {
    Destroy(FINAL_STATUS_RECENTLY_VISITED);
    return false;
  }
  return true;
}

bool PrerenderContents::AddAliasURL(const GURL& url) {
  if (!CheckURL(url))
    return false;

  alias_urls_.push_back(url);

  for (content::RenderProcessHost::iterator host_iterator =
           content::RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd();
       host_iterator.Advance()) {
    content::RenderProcessHost* host = host_iterator.GetCurrentValue();
    IPC::ChannelProxy* channel = host->GetChannel();
    // |channel| might be NULL in tests.
    if (channel) {
      chrome::mojom::PrerenderDispatcherAssociatedPtr prerender_dispatcher;
      channel->GetRemoteAssociatedInterface(&prerender_dispatcher);
      prerender_dispatcher->PrerenderAddAlias(url);
    }
  }

  return true;
}

bool PrerenderContents::Matches(
    const GURL& url,
    const SessionStorageNamespace* session_storage_namespace) const {
  // TODO(davidben): Remove any consumers that pass in a NULL
  // session_storage_namespace and only test with matches.
  if (session_storage_namespace &&
      session_storage_namespace_id_ != session_storage_namespace->id()) {
    return false;
  }
  return base::ContainsValue(alias_urls_, url);
}

void PrerenderContents::RenderProcessGone(base::TerminationStatus status) {
  if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
    // The renderer process is being killed because of the browser/test
    // shutdown, before the termination notification is received.
    Destroy(FINAL_STATUS_APP_TERMINATING);
  }
  Destroy(FINAL_STATUS_RENDERER_CRASHED);
}

void PrerenderContents::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void PrerenderContents::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // When a new RenderFrame is created for a prerendering WebContents, tell the
  // new RenderFrame it's being used for prerendering before any navigations
  // occur.  Note that this is always triggered before the first navigation, so
  // there's no need to send the message just after the WebContents is created.
  render_frame_host->Send(new PrerenderMsg_SetIsPrerendering(
      render_frame_host->GetRoutingID(), prerender_mode_,
      PrerenderHistograms::GetHistogramPrefix(origin_)));
}

void PrerenderContents::DidStopLoading() {
  NotifyPrerenderStopLoading();
}

void PrerenderContents::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent())
    NotifyPrerenderDomContentLoaded();
}

void PrerenderContents::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!CheckURL(navigation_handle->GetURL()))
    return;

  // Usually, this event fires if the user clicks or enters a new URL.
  // Neither of these can happen in the case of an invisible prerender.
  // So the cause is: Some JavaScript caused a new URL to be loaded.  In that
  // case, the spinner would start again in the browser, so we must reset
  // has_finished_loading_ so that the spinner won't be stopped.
  has_finished_loading_ = false;
}

void PrerenderContents::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  // If it's a redirect on the top-level resource, the name needs to be
  // remembered for future matching, and if it redirects to an https resource,
  // it needs to be canceled. If a subresource is redirected, nothing changes.
  CheckURL(navigation_handle->GetURL());
}

void PrerenderContents::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->GetParent())
    has_finished_loading_ = true;
}

void PrerenderContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  if (navigation_handle->GetResponseHeaders() &&
      navigation_handle->GetResponseHeaders()->response_code() >= 400) {
    // Maintain same behavior as old navigation API when the URL is unreachable
    // and leads to an error page. While there will be a subsequent navigation
    // that has navigation_handle->IsErrorPage(), it'll be too late to wait for
    // it as the renderer side will consider this prerender complete. This
    // object would therefore have been destructed already and so instead look
    // for the error response code now.
    // Also maintain same final status code that previous navigation API
    // returned, which was reached because the URL for the error page was
    // kUnreachableWebDataURL and that was interpreted as unsupported scheme.
    Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
    return;
  }

  // If the prerender made a second navigation entry, abort the prerender. This
  // avoids having to correctly implement a complex history merging case (this
  // interacts with location.replace) and correctly synchronize with the
  // renderer. The final status may be monitored to see we need to revisit this
  // decision. This does not affect client redirects as those do not push new
  // history entries. (Calls to location.replace, navigations before onload, and
  // <meta http-equiv=refresh> with timeouts under 1 second do not create
  // entries in Blink.)
  if (prerender_contents_->GetController().GetEntryCount() > 1) {
    Destroy(FINAL_STATUS_NEW_NAVIGATION_ENTRY);
    return;
  }

  // Add each redirect as an alias. |navigation_handle->GetURL()| is included in
  // |navigation_handle->GetRedirectChain()|.
  //
  // TODO(davidben): We do not correctly patch up history for renderer-initated
  // navigations which add history entries. http://crbug.com/305660.
  for (const auto& redirect : navigation_handle->GetRedirectChain()) {
    if (!AddAliasURL(redirect))
      return;
  }
}

void PrerenderContents::Destroy(FinalStatus final_status) {
  DCHECK_NE(final_status, FINAL_STATUS_USED);

  if (prerendering_has_been_cancelled_)
    return;

  SetFinalStatus(final_status);

  prerendering_has_been_cancelled_ = true;
  prerender_manager_->AddToHistory(this);
  prerender_manager_->SetPrefetchFinalStatusForUrl(prerender_url_,
                                                   final_status);
  prerender_manager_->MoveEntryToPendingDelete(this, final_status);

  if (prerendering_has_started())
    NotifyPrerenderStop();
}

void PrerenderContents::DestroyWhenUsingTooManyResources() {
  if (process_pid_ == base::kNullProcessId) {
    const RenderViewHost* rvh = GetRenderViewHost();
    if (!rvh)
      return;

    const content::RenderProcessHost* rph = rvh->GetProcess();
    if (!rph)
      return;

    base::ProcessHandle handle = rph->GetProcess().Handle();
    if (handle == base::kNullProcessHandle)
      return;

    process_pid_ = rph->GetProcess().Pid();
  }

  if (process_pid_ == base::kNullProcessId)
    return;

  // Using AdaptCallbackForRepeating allows for an easier transition to
  // OnceCallbacks for https://crbug.com/714018.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          process_pid_, base::AdaptCallbackForRepeating(base::BindOnce(
                            &PrerenderContents::DidGetMemoryUsage,
                            weak_factory_.GetWeakPtr())));
}

void PrerenderContents::DidGetMemoryUsage(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> global_dump) {
  if (!success)
    return;

  for (const memory_instrumentation::GlobalMemoryDump::ProcessDump& dump :
       global_dump->process_dumps()) {
    if (dump.pid() != process_pid_)
      continue;

    // If |final_status_| == |FINAL_STATUS_USED|, then destruction will be
    // handled by the entity that set final_status_.
    if (dump.os_dump().private_footprint_kb * 1024 >
            prerender_manager_->config().max_bytes &&
        final_status_ != FINAL_STATUS_USED) {
      Destroy(FINAL_STATUS_MEMORY_LIMIT_EXCEEDED);
    }
    return;
  }
}

std::unique_ptr<WebContents> PrerenderContents::ReleasePrerenderContents() {
  prerender_contents_->SetDelegate(nullptr);
  content::WebContentsObserver::Observe(nullptr);

  // Clear the task manager tag we added earlier to our
  // WebContents since it's no longer a prerender contents.
  task_manager::WebContentsTags::ClearTag(prerender_contents_.get());

  return std::move(prerender_contents_);
}

RenderViewHost* PrerenderContents::GetRenderViewHostMutable() {
  return const_cast<RenderViewHost*>(GetRenderViewHost());
}

const RenderViewHost* PrerenderContents::GetRenderViewHost() const {
  return prerender_contents_ ? prerender_contents_->GetRenderViewHost()
                             : nullptr;
}

void PrerenderContents::DidNavigate(
    const history::HistoryAddPageArgs& add_page_args) {
  add_page_vector_.push_back(add_page_args);
}

void PrerenderContents::CommitHistory(WebContents* tab) {
  HistoryTabHelper* history_tab_helper = HistoryTabHelper::FromWebContents(tab);
  for (size_t i = 0; i < add_page_vector_.size(); ++i)
    history_tab_helper->UpdateHistoryForNavigation(add_page_vector_[i]);
}

std::unique_ptr<base::DictionaryValue> PrerenderContents::GetAsValue() const {
  if (!prerender_contents_)
    return nullptr;
  auto dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->SetString("url", prerender_url_.spec());
  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta duration = current_time - load_start_time_;
  dict_value->SetInteger("duration", duration.InSeconds());
  dict_value->SetBoolean("is_loaded", prerender_contents_ &&
                                      !prerender_contents_->IsLoading());
  return dict_value;
}

void PrerenderContents::PrepareForUse() {
  SetFinalStatus(FINAL_STATUS_USED);

  if (prerender_contents_.get()) {
    prerender_contents_->SendToAllFrames(new PrerenderMsg_SetIsPrerendering(
        MSG_ROUTING_NONE, NO_PRERENDER, std::string()));
  }

  NotifyPrerenderStop();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ResumeThrottles, resource_throttles_, idle_resources_));
  resource_throttles_.clear();
  idle_resources_.clear();
}

void PrerenderContents::CancelPrerenderForPrinting() {
  Destroy(FINAL_STATUS_WINDOW_PRINT);
}

void PrerenderContents::CancelPrerenderForUnsupportedMethod() {
  Destroy(FINAL_STATUS_INVALID_HTTP_METHOD);
}

void PrerenderContents::CancelPrerenderForUnsupportedScheme(const GURL& url) {
  Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
  ReportUnsupportedPrerenderScheme(url);
}

void PrerenderContents::CancelPrerenderForSyncDeferredRedirect() {
  Destroy(FINAL_STATUS_BAD_DEFERRED_REDIRECT);
}

void PrerenderContents::OnPrerenderCancelerRequest(
    chrome::mojom::PrerenderCancelerRequest request) {
  if (!prerender_canceler_binding_.is_bound())
    prerender_canceler_binding_.Bind(std::move(request));
}

void PrerenderContents::AddResourceThrottle(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle) {
  resource_throttles_.push_back(throttle);
}

void PrerenderContents::AddIdleResource(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle) {
  idle_resources_.push_back(throttle);
}

void PrerenderContents::AddNetworkBytes(int64_t bytes) {
  network_bytes_ += bytes;
  for (Observer& observer : observer_list_)
    observer.OnPrerenderNetworkBytesChanged(this);
}

}  // namespace prerender
