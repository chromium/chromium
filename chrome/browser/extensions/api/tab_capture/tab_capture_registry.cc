// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"
#include "url/origin.h"

using content::BrowserThread;
using extensions::tab_capture::TabCaptureState;

namespace extensions {

namespace tab_capture = api::tab_capture;

// Stores values associated with a tab capture request, maintains lifecycle
// state, and monitors WebContents for fullscreen transition events and
// destruction.
class TabCaptureRegistry::LiveRequest : public content::WebContentsObserver {
 public:
  LiveRequest(content::WebContents* target_contents,
              const std::string& extension_id,
              bool is_anonymous,
              TabCaptureRegistry* registry)
      : content::WebContentsObserver(target_contents),
        extension_id_(extension_id),
        is_anonymous_(is_anonymous),
        registry_(registry),
        capture_state_(tab_capture::TAB_CAPTURE_STATE_NONE),
        is_verified_(false),
        // TODO(miu): This initial value for |is_fullscreened_| is a faulty
        // assumption.  http://crbug.com/350491
        is_fullscreened_(false),
        render_process_id_(
            target_contents->GetMainFrame()->GetProcess()->GetID()),
        render_frame_id_(target_contents->GetMainFrame()->GetRoutingID()) {
    DCHECK(web_contents());
    DCHECK(registry_);
  }

  ~LiveRequest() override {}

  // Accessors.
  const std::string& extension_id() const { return extension_id_; }
  bool is_anonymous() const { return is_anonymous_; }
  TabCaptureState capture_state() const { return capture_state_; }
  bool is_verified() const { return is_verified_; }

  void SetIsVerified() {
    DCHECK(!is_verified_);
    is_verified_ = true;
  }

  bool WasTargettingRenderFrameID(int render_process_id,
                                  int render_frame_id) const {
    return render_process_id_ == render_process_id &&
           render_frame_id_ == render_frame_id;
  }

  void UpdateCaptureState(TabCaptureState next_capture_state) {
    // This method can get duplicate calls if both audio and video were
    // requested, so return early to avoid duplicate dispatching of status
    // change events.
    if (capture_state_ == next_capture_state)
      return;

    capture_state_ = next_capture_state;
    registry_->DispatchStatusChangeEvent(this);
  }

  void GetCaptureInfo(tab_capture::CaptureInfo* info) const {
    info->tab_id = SessionTabHelper::IdForTab(web_contents()).id();
    info->status = capture_state_;
    info->fullscreen = is_fullscreened_;
  }

 protected:
  void DidShowFullscreenWidget() override {
    is_fullscreened_ = true;
    if (capture_state_ == tab_capture::TAB_CAPTURE_STATE_ACTIVE)
      registry_->DispatchStatusChangeEvent(this);
  }

  void DidDestroyFullscreenWidget() override {
    is_fullscreened_ = false;
    if (capture_state_ == tab_capture::TAB_CAPTURE_STATE_ACTIVE)
      registry_->DispatchStatusChangeEvent(this);
  }

  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    is_fullscreened_ = entered_fullscreen;
    if (capture_state_ == tab_capture::TAB_CAPTURE_STATE_ACTIVE)
      registry_->DispatchStatusChangeEvent(this);
  }

  void WebContentsDestroyed() override {
    registry_->KillRequest(this);  // Deletes |this|.
  }

 private:
  const std::string extension_id_;
  const bool is_anonymous_;
  TabCaptureRegistry* const registry_;
  TabCaptureState capture_state_;
  bool is_verified_;
  bool is_fullscreened_;

  // These reference the originally targetted RenderFrameHost by its ID.  The
  // RenderFrameHost may have gone away long before a LiveRequest closes, but
  // calls to OnRequestUpdate() will always refer to this request by this ID.
  int render_process_id_;
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(LiveRequest);
};

TabCaptureRegistry::TabCaptureRegistry(content::BrowserContext* context)
    : browser_context_(context) {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

TabCaptureRegistry::~TabCaptureRegistry() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

// static
TabCaptureRegistry* TabCaptureRegistry::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<TabCaptureRegistry>::Get(context);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TabCaptureRegistry>>::
    DestructorAtExit g_tab_capture_registry_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TabCaptureRegistry>*
TabCaptureRegistry::GetFactoryInstance() {
  return g_tab_capture_registry_factory.Pointer();
}

void TabCaptureRegistry::GetCapturedTabs(
    const std::string& extension_id,
    base::ListValue* list_of_capture_info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(list_of_capture_info);
  list_of_capture_info->Clear();
  for (const std::unique_ptr<LiveRequest>& request : requests_) {
    if (request->is_anonymous() || !request->is_verified() ||
        request->extension_id() != extension_id)
      continue;
    tab_capture::CaptureInfo info;
    request->GetCaptureInfo(&info);
    list_of_capture_info->Append(info.ToValue());
  }
}

void TabCaptureRegistry::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Cleanup all the requested media streams for this extension.
  for (auto it = requests_.begin(); it != requests_.end();) {
    if ((*it)->extension_id() == extension->id()) {
      it = requests_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string TabCaptureRegistry::AddRequest(
    content::WebContents* target_contents,
    const std::string& extension_id,
    bool is_anonymous,
    const GURL& origin,
    content::DesktopMediaID source,
    const std::string& extension_name,
    content::WebContents* caller_contents) {
  std::string device_id;
  LiveRequest* const request = FindRequest(target_contents);

  // Currently, we do not allow multiple active captures for same tab.
  if (request != NULL) {
    if (request->capture_state() == tab_capture::TAB_CAPTURE_STATE_PENDING ||
        request->capture_state() == tab_capture::TAB_CAPTURE_STATE_ACTIVE) {
      return device_id;
    } else {
      // Delete the request before creating its replacement (below).
      KillRequest(request);
    }
  }

  requests_.push_back(std::make_unique<LiveRequest>(
      target_contents, extension_id, is_anonymous, this));

  content::RenderFrameHost* const main_frame = caller_contents->GetMainFrame();
  if (main_frame) {
    device_id = content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
        main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(),
        url::Origin::Create(origin), source, extension_name,
        content::kRegistryStreamTypeTab);
  }

  return device_id;
}

bool TabCaptureRegistry::VerifyRequest(int render_process_id,
                                       int render_frame_id,
                                       const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LiveRequest* const request = FindRequest(render_process_id, render_frame_id);
  if (!request) {
    return false;  // Unknown RenderFrameHost ID, or frame has gone away.
  }

  if (request->is_verified() ||
      (request->capture_state() != tab_capture::TAB_CAPTURE_STATE_NONE &&
       request->capture_state() != tab_capture::TAB_CAPTURE_STATE_PENDING))
    return false;

  request->SetIsVerified();
  return true;
}

void TabCaptureRegistry::OnRequestUpdate(
    int target_render_process_id,
    int target_render_frame_id,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (stream_type != blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE &&
      stream_type != blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
    return;
  }

  LiveRequest* request =
      FindRequest(target_render_process_id, target_render_frame_id);
  if (!request) {
    return;  // Stale or invalid request update.
  }

  TabCaptureState next_state = tab_capture::TAB_CAPTURE_STATE_NONE;
  switch (new_state) {
    case content::MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      next_state = tab_capture::TAB_CAPTURE_STATE_PENDING;
      break;
    case content::MEDIA_REQUEST_STATE_DONE:
      next_state = tab_capture::TAB_CAPTURE_STATE_ACTIVE;
      break;
    case content::MEDIA_REQUEST_STATE_CLOSING:
      next_state = tab_capture::TAB_CAPTURE_STATE_STOPPED;
      break;
    case content::MEDIA_REQUEST_STATE_ERROR:
      next_state = tab_capture::TAB_CAPTURE_STATE_ERROR;
      break;
    case content::MEDIA_REQUEST_STATE_OPENING:
      return;
    case content::MEDIA_REQUEST_STATE_REQUESTED:
    case content::MEDIA_REQUEST_STATE_NOT_REQUESTED:
      NOTREACHED();
      return;
  }

  if (next_state == tab_capture::TAB_CAPTURE_STATE_PENDING &&
      request->capture_state() != tab_capture::TAB_CAPTURE_STATE_PENDING &&
      request->capture_state() != tab_capture::TAB_CAPTURE_STATE_NONE &&
      request->capture_state() != tab_capture::TAB_CAPTURE_STATE_STOPPED &&
      request->capture_state() != tab_capture::TAB_CAPTURE_STATE_ERROR) {
    // If we end up trying to grab a new stream while the previous one was never
    // terminated, then something fishy is going on.
    NOTREACHED() << "Trying to capture tab with existing stream.";
    return;
  }

  request->UpdateCaptureState(next_state);
}

void TabCaptureRegistry::DispatchStatusChangeEvent(
    const LiveRequest* request) const {
  if (request->is_anonymous())
    return;

  EventRouter* router = EventRouter::Get(browser_context_);
  if (!router)
    return;

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  tab_capture::CaptureInfo info;
  request->GetCaptureInfo(&info);
  args->Append(info.ToValue());
  auto event = std::make_unique<Event>(events::TAB_CAPTURE_ON_STATUS_CHANGED,
                                       tab_capture::OnStatusChanged::kEventName,
                                       std::move(args), browser_context_);

  router->DispatchEventToExtension(request->extension_id(), std::move(event));
}

TabCaptureRegistry::LiveRequest* TabCaptureRegistry::FindRequest(
    const content::WebContents* target_contents) const {
  for (const auto& request : requests_) {
    if (request->web_contents() == target_contents)
      return request.get();
  }
  return nullptr;
}

TabCaptureRegistry::LiveRequest* TabCaptureRegistry::FindRequest(
    int target_render_process_id,
    int target_render_frame_id) const {
  for (const std::unique_ptr<LiveRequest>& request : requests_) {
    if (request->WasTargettingRenderFrameID(target_render_process_id,
                                            target_render_frame_id)) {
      return request.get();
    }
  }
  return nullptr;
}

void TabCaptureRegistry::KillRequest(LiveRequest* request) {
  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    if (it->get() == request) {
      requests_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace extensions
