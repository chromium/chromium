// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
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
              const ExtensionId& extension_id,
              bool is_anonymous,
              TabCaptureRegistry* registry)
      : content::WebContentsObserver(target_contents),
        extension_id_(extension_id),
        is_anonymous_(is_anonymous),
        registry_(registry),
        render_process_id_(
            target_contents->GetPrimaryMainFrame()->GetProcess()->GetID()),
        render_frame_id_(
            target_contents->GetPrimaryMainFrame()->GetRoutingID()) {
    DCHECK(web_contents());
    DCHECK(registry_);
  }

  LiveRequest(const LiveRequest&) = delete;
  LiveRequest& operator=(const LiveRequest&) = delete;

  ~LiveRequest() override {}

  // Accessors.
  const ExtensionId& extension_id() const { return extension_id_; }
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
    info->tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
    info->status = capture_state_;
    info->fullscreen = is_fullscreened_;
  }

 protected:
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    is_fullscreened_ = entered_fullscreen;
    if (capture_state_ == tab_capture::TabCaptureState::kActive) {
      registry_->DispatchStatusChangeEvent(this);
    }
  }

  void WebContentsDestroyed() override {
    registry_->KillRequest(this);  // Deletes |this|.
  }

 private:
  const ExtensionId extension_id_;
  const bool is_anonymous_;
  const raw_ptr<TabCaptureRegistry> registry_;
  TabCaptureState capture_state_ = tab_capture::TabCaptureState::kNone;
  bool is_verified_ = false;
  bool is_fullscreened_ = false;

  // These reference the originally targetted RenderFrameHost by its ID.  The
  // RenderFrameHost may have gone away long before a LiveRequest closes, but
  // calls to OnRequestUpdate() will always refer to this request by this ID.
  int render_process_id_;
  int render_frame_id_;
};

TabCaptureRegistry::TabCaptureRegistry(content::BrowserContext* context)
    : browser_context_(context) {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
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
    base::Value::List* capture_info_list) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(capture_info_list);
  capture_info_list->clear();
  for (const std::unique_ptr<LiveRequest>& request : requests_) {
    if (request->is_anonymous() || !request->is_verified() ||
        request->extension_id() != extension_id)
      continue;
    tab_capture::CaptureInfo info;
    request->GetCaptureInfo(&info);
    capture_info_list->Append(info.ToValue());
  }
}

void TabCaptureRegistry::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // Cleanup all the requested media streams for this extension.
  std::erase_if(requests_, [extension](const auto& entry) {
    return entry->extension_id() == extension->id();
  });
}

std::string TabCaptureRegistry::AddRequest(
    content::WebContents* target_contents,
    const std::string& extension_id,
    bool is_anonymous,
    const GURL& origin,
    content::DesktopMediaID source,
    int caller_render_process_id,
    std::optional<int> restrict_to_render_frame_id) {
  std::string device_id;
  LiveRequest* const request = FindRequest(target_contents);

  // Currently, we do not allow multiple active captures for same tab.
  if (request != nullptr) {
    if (request->capture_state() == tab_capture::TabCaptureState::kPending ||
        request->capture_state() == tab_capture::TabCaptureState::kActive) {
      return device_id;
    } else {
      // Delete the request before creating its replacement (below).
      KillRequest(request);
    }
  }

  requests_.push_back(std::make_unique<LiveRequest>(
      target_contents, extension_id, is_anonymous, this));

  device_id = content::DesktopStreamsRegistry::GetInstance()->RegisterStream(
      caller_render_process_id, restrict_to_render_frame_id,
      url::Origin::Create(origin), source, content::kRegistryStreamTypeTab);

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
      (request->capture_state() != tab_capture::TabCaptureState::kNone &&
       request->capture_state() != tab_capture::TabCaptureState::kPending)) {
    return false;
  }

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

  TabCaptureState next_state = tab_capture::TabCaptureState::kNone;
  switch (new_state) {
    case content::MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      next_state = tab_capture::TabCaptureState::kPending;
      break;
    case content::MEDIA_REQUEST_STATE_DONE:
      next_state = tab_capture::TabCaptureState::kActive;
      break;
    case content::MEDIA_REQUEST_STATE_CLOSING:
      next_state = tab_capture::TabCaptureState::kStopped;
      break;
    case content::MEDIA_REQUEST_STATE_ERROR:
      next_state = tab_capture::TabCaptureState::kError;
      break;
    case content::MEDIA_REQUEST_STATE_OPENING:
      return;
    case content::MEDIA_REQUEST_STATE_REQUESTED:
    case content::MEDIA_REQUEST_STATE_NOT_REQUESTED:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  if (next_state == tab_capture::TabCaptureState::kPending &&
      request->capture_state() != tab_capture::TabCaptureState::kPending &&
      request->capture_state() != tab_capture::TabCaptureState::kNone &&
      request->capture_state() != tab_capture::TabCaptureState::kStopped &&
      request->capture_state() != tab_capture::TabCaptureState::kError) {
    // Despite other code preventing multiple captures of the same tab, we can
    // reach this case due to a race condition (see crbug.com/1370338).
    // TODO(crbug.com/40874553): Handle status updates for multiple capturers.
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

  base::Value::List args;
  tab_capture::CaptureInfo info;
  request->GetCaptureInfo(&info);
  args.Append(info.ToValue());
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
  NOTREACHED_IN_MIGRATION();
}

}  // namespace extensions
