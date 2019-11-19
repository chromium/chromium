// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_permission_manager.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using blink::mojom::PermissionStatus;
using content::PermissionType;

using RequestPermissionsCallback =
    base::OnceCallback<void(const std::vector<PermissionStatus>&)>;

namespace android_webview {

namespace {

void PermissionRequestResponseCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  std::move(callback).Run(vector.at(0));
}

}  // namespace

class LastRequestResultCache {
 public:
  LastRequestResultCache() = default;

  void SetResult(PermissionType permission,
                 const GURL& requesting_origin,
                 const GURL& embedding_origin,
                 PermissionStatus status) {
    DCHECK(status == PermissionStatus::GRANTED ||
           status == PermissionStatus::DENIED);

    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      DLOG(WARNING) << "Not caching result because of empty origin.";
      return;
    }

    if (!requesting_origin.is_valid()) {
      NOTREACHED() << requesting_origin.possibly_invalid_spec();
      return;
    }
    if (!embedding_origin.is_valid()) {
      NOTREACHED() << embedding_origin.possibly_invalid_spec();
      return;
    }

    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      // Other permissions are not cached.
      return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    if (key.empty()) {
      NOTREACHED();
      // Never store an empty key because it could inadvertently be used for
      // another combination.
      return;
    }
    pmi_result_cache_[key] = status;
  }

  PermissionStatus GetResult(PermissionType permission,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin) const {
    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      return PermissionStatus::ASK;
    }

    DCHECK(requesting_origin.is_valid())
        << requesting_origin.possibly_invalid_spec();
    DCHECK(embedding_origin.is_valid())
        << embedding_origin.possibly_invalid_spec();

    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      NOTREACHED() << "Results are only cached for PROTECTED_MEDIA_IDENTIFIER";
      return PermissionStatus::ASK;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    StatusMap::const_iterator it = pmi_result_cache_.find(key);
    if (it == pmi_result_cache_.end()) {
      DLOG(WARNING) << "GetResult() called for uncached origins: " << key;
      return PermissionStatus::ASK;
    }

    DCHECK(!key.empty());
    return it->second;
  }

  void ClearResult(PermissionType permission,
                   const GURL& requesting_origin,
                   const GURL& embedding_origin) {
    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      return;
    }

    DCHECK(requesting_origin.is_valid())
        << requesting_origin.possibly_invalid_spec();
    DCHECK(embedding_origin.is_valid())
        << embedding_origin.possibly_invalid_spec();


    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      // Other permissions are not cached, so nothing to clear.
      return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    pmi_result_cache_.erase(key);
  }

 private:
  // Returns a concatenation of the origins to be used as the index.
  // Returns the empty string if either origin is invalid or empty.
  static std::string GetCacheKey(const GURL& requesting_origin,
                                 const GURL& embedding_origin) {
    const std::string& requesting = requesting_origin.spec();
    const std::string& embedding = embedding_origin.spec();
    if (requesting.empty() || embedding.empty())
      return std::string();
    return requesting + "," + embedding;
  }

  using StatusMap = std::unordered_map<std::string, PermissionStatus>;
  StatusMap pmi_result_cache_;

  DISALLOW_COPY_AND_ASSIGN(LastRequestResultCache);
};

class AwPermissionManager::PendingRequest {
 public:
  PendingRequest(const std::vector<PermissionType> permissions,
                 GURL requesting_origin,
                 GURL embedding_origin,
                 int render_process_id,
                 int render_frame_id,
                 RequestPermissionsCallback callback)
      : permissions(permissions),
        requesting_origin(requesting_origin),
        embedding_origin(embedding_origin),
        render_process_id(render_process_id),
        render_frame_id(render_frame_id),
        callback(std::move(callback)),
        results(permissions.size(), PermissionStatus::DENIED),
        cancelled_(false) {
    for (size_t i = 0; i < permissions.size(); ++i)
      permission_index_map_.insert(std::make_pair(permissions[i], i));
  }

  ~PendingRequest() = default;

  void SetPermissionStatus(PermissionType type, PermissionStatus status) {
    auto result = permission_index_map_.find(type);
    if (result == permission_index_map_.end()) {
      NOTREACHED();
      return;
    }
    DCHECK(!IsCompleted());
    results[result->second] = status;
    if (type == PermissionType::MIDI_SYSEX &&
        status == PermissionStatus::GRANTED) {
      content::ChildProcessSecurityPolicy::GetInstance()
          ->GrantSendMidiSysExMessage(render_process_id);
    }
    resolved_permissions_.insert(type);
  }

  PermissionStatus GetPermissionStatus(PermissionType type) {
    auto result = permission_index_map_.find(type);
    if (result == permission_index_map_.end()) {
      NOTREACHED();
      return PermissionStatus::DENIED;
    }
    return results[result->second];
  }

  bool HasPermissionType(PermissionType type) {
    return permission_index_map_.find(type) != permission_index_map_.end();
  }

  bool IsCompleted() const {
    return results.size() == resolved_permissions_.size();
  }

  bool IsCompleted(PermissionType type) const {
    return resolved_permissions_.count(type) != 0;
  }

  void Cancel() { cancelled_ = true; }

  bool IsCancelled() const { return cancelled_; }

  std::vector<PermissionType> permissions;
  GURL requesting_origin;
  GURL embedding_origin;
  int render_process_id;
  int render_frame_id;
  RequestPermissionsCallback callback;
  std::vector<PermissionStatus> results;

 private:
  std::map<PermissionType, size_t> permission_index_map_;
  std::set<PermissionType> resolved_permissions_;
  bool cancelled_;
};

AwPermissionManager::AwPermissionManager()
    : result_cache_(new LastRequestResultCache) {}

AwPermissionManager::~AwPermissionManager() {
  CancelPermissionRequests();
}

int AwPermissionManager::RequestPermission(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(PermissionStatus)> callback) {
  return RequestPermissions(
      std::vector<PermissionType>(1, permission), render_frame_host,
      requesting_origin, user_gesture,
      base::BindOnce(&PermissionRequestResponseCallbackWrapper,
                     std::move(callback)));
}

int AwPermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  if (permissions.empty()) {
    std::move(callback).Run(std::vector<PermissionStatus>());
    return content::PermissionController::kNoPendingOperation;
  }

  const GURL& embedding_origin = LastCommittedOrigin(render_frame_host);

  auto pending_request = std::make_unique<PendingRequest>(
      permissions, requesting_origin, embedding_origin,
      GetRenderProcessID(render_frame_host),
      GetRenderFrameID(render_frame_host), std::move(callback));
  std::vector<bool> should_delegate_requests =
      std::vector<bool>(permissions.size(), true);
  for (size_t i = 0; i < permissions.size(); ++i) {
    for (PendingRequestsMap::Iterator<PendingRequest> it(&pending_requests_);
         !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue()->HasPermissionType(permissions[i]) &&
          it.GetCurrentValue()->requesting_origin == requesting_origin) {
        if (it.GetCurrentValue()->IsCompleted(permissions[i])) {
          pending_request->SetPermissionStatus(
              permissions[i],
              it.GetCurrentValue()->GetPermissionStatus(permissions[i]));
        }
        should_delegate_requests[i] = false;
        break;
      }
    }
  }

  // Keep copy of pointer for performing further operations after ownership is
  // transferred to pending_requests_
  PendingRequest* pending_request_raw = pending_request.get();
  const int request_id = pending_requests_.Add(std::move(pending_request));

  AwBrowserPermissionRequestDelegate* delegate =
      GetDelegate(pending_request_raw->render_process_id,
                  pending_request_raw->render_frame_id);

  for (size_t i = 0; i < permissions.size(); ++i) {
    if (!should_delegate_requests[i])
      continue;

    if (!delegate) {
      DVLOG(0) << "Dropping permissions request for "
               << static_cast<int>(permissions[i]);
      pending_request_raw->SetPermissionStatus(permissions[i],
                                               PermissionStatus::DENIED);
      continue;
    }

    switch (permissions[i]) {
      case PermissionType::GEOLOCATION:
        delegate->RequestGeolocationPermission(
            pending_request_raw->requesting_origin,
            base::BindOnce(&OnRequestResponse, weak_ptr_factory_.GetWeakPtr(),
                           request_id, permissions[i]));
        break;
      case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
        delegate->RequestProtectedMediaIdentifierPermission(
            pending_request_raw->requesting_origin,
            base::BindOnce(&OnRequestResponse, weak_ptr_factory_.GetWeakPtr(),
                           request_id, permissions[i]));
        break;
      case PermissionType::MIDI_SYSEX:
        delegate->RequestMIDISysexPermission(
            pending_request_raw->requesting_origin,
            base::BindOnce(&OnRequestResponse, weak_ptr_factory_.GetWeakPtr(),
                           request_id, permissions[i]));
        break;
      case PermissionType::AUDIO_CAPTURE:
      case PermissionType::VIDEO_CAPTURE:
      case PermissionType::NOTIFICATIONS:
      case PermissionType::DURABLE_STORAGE:
      case PermissionType::BACKGROUND_SYNC:
      case PermissionType::FLASH:
      case PermissionType::ACCESSIBILITY_EVENTS:
      case PermissionType::CLIPBOARD_READ:
      case PermissionType::CLIPBOARD_WRITE:
      case PermissionType::PAYMENT_HANDLER:
      case PermissionType::BACKGROUND_FETCH:
      case PermissionType::IDLE_DETECTION:
      case PermissionType::PERIODIC_BACKGROUND_SYNC:
      case PermissionType::NFC:
        NOTIMPLEMENTED() << "RequestPermissions is not implemented for "
                         << static_cast<int>(permissions[i]);
        pending_request_raw->SetPermissionStatus(permissions[i],
                                                 PermissionStatus::DENIED);
        break;
      case PermissionType::MIDI:
      case PermissionType::SENSORS:
      case PermissionType::WAKE_LOCK_SCREEN:
        // PermissionType::SENSORS requests are always granted so that access
        // to device motion and device orientation data (and underlying
        // sensors) works in the WebView. SensorProviderImpl::GetSensor()
        // filters requests for other types of sensors.
        pending_request_raw->SetPermissionStatus(permissions[i],
                                                 PermissionStatus::GRANTED);
        break;
      case PermissionType::WAKE_LOCK_SYSTEM:
        pending_request_raw->SetPermissionStatus(permissions[i],
                                                 PermissionStatus::DENIED);
        break;
      case PermissionType::NUM:
        NOTREACHED() << "PermissionType::NUM was not expected here.";
        pending_request_raw->SetPermissionStatus(permissions[i],
                                                 PermissionStatus::DENIED);
        break;
    }
  }

  // If delegate resolve the permission synchronously, all requests could be
  // already resolved here.
  if (!pending_requests_.Lookup(request_id))
    return content::PermissionController::kNoPendingOperation;

  // If requests are resolved without calling delegate functions, e.g.
  // PermissionType::MIDI is permitted within the previous for-loop, all
  // requests could be already resolved, but still in the |pending_requests_|
  // without invoking the callback.
  if (pending_request_raw->IsCompleted()) {
    std::vector<PermissionStatus> results = pending_request_raw->results;
    RequestPermissionsCallback completed_callback =
        std::move(pending_request_raw->callback);
    pending_requests_.Remove(request_id);
    std::move(completed_callback).Run(results);
    return content::PermissionController::kNoPendingOperation;
  }

  return request_id;
}

// static
void AwPermissionManager::OnRequestResponse(
    const base::WeakPtr<AwPermissionManager>& manager,
    int request_id,
    PermissionType permission,
    bool allowed) {
  // All delegate functions should be cancelled when the manager runs
  // destructor. Therefore |manager| should be always valid here.
  DCHECK(manager);

  PermissionStatus status =
      allowed ? PermissionStatus::GRANTED : PermissionStatus::DENIED;
  PendingRequest* pending_request =
      manager->pending_requests_.Lookup(request_id);

  manager->result_cache_->SetResult(permission,
                                    pending_request->requesting_origin,
                                    pending_request->embedding_origin, status);

  std::vector<int> complete_request_ids;
  std::vector<
      std::pair<RequestPermissionsCallback, std::vector<PermissionStatus>>>
      complete_request_pairs;
  for (PendingRequestsMap::Iterator<PendingRequest> it(
           &manager->pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    if (!it.GetCurrentValue()->HasPermissionType(permission) ||
        it.GetCurrentValue()->requesting_origin !=
            pending_request->requesting_origin) {
      continue;
    }
    it.GetCurrentValue()->SetPermissionStatus(permission, status);
    if (it.GetCurrentValue()->IsCompleted()) {
      complete_request_ids.push_back(it.GetCurrentKey());
      if (!it.GetCurrentValue()->IsCancelled()) {
        complete_request_pairs.emplace_back(
            std::move(it.GetCurrentValue()->callback),
            std::move(it.GetCurrentValue()->results));
      }
    }
  }
  for (auto id : complete_request_ids)
    manager->pending_requests_.Remove(id);
  for (auto& pair : complete_request_pairs)
    std::move(pair.first).Run(pair.second);
}

void AwPermissionManager::ResetPermission(PermissionType permission,
                                          const GURL& requesting_origin,
                                          const GURL& embedding_origin) {
  result_cache_->ClearResult(permission, requesting_origin, embedding_origin);
}

PermissionStatus AwPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Method is called outside the Permissions API only for this permission.
  if (permission == PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
    return result_cache_->GetResult(permission, requesting_origin,
                                    embedding_origin);
  } else if (permission == PermissionType::MIDI ||
             permission == PermissionType::SENSORS) {
    return PermissionStatus::GRANTED;
  }

  return PermissionStatus::DENIED;
}

PermissionStatus AwPermissionManager::GetPermissionStatusForFrame(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  return GetPermissionStatus(
      permission, requesting_origin,
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetLastCommittedURL()
          .GetOrigin());
}

int AwPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(PermissionStatus)> callback) {
  return content::PermissionController::kNoPendingOperation;
}

void AwPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {}

void AwPermissionManager::CancelPermissionRequest(int request_id) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  if (!pending_request || pending_request->IsCancelled())
    return;
  pending_request->Cancel();

  const GURL& embedding_origin = pending_request->embedding_origin;
  const GURL& requesting_origin = pending_request->requesting_origin;
  for (auto permission : pending_request->permissions)
    result_cache_->ClearResult(permission, requesting_origin, embedding_origin);

  AwBrowserPermissionRequestDelegate* delegate = GetDelegate(
      pending_request->render_process_id, pending_request->render_frame_id);

  for (auto permission : pending_request->permissions) {
    // If the permission was already resolved, we do not need to cancel it.
    if (pending_request->IsCompleted(permission))
      continue;

    // If another pending_request waits for the same permission being resolved,
    // we should not cancel the delegate's request.
    bool should_not_cancel_ = false;
    for (PendingRequestsMap::Iterator<PendingRequest> it(&pending_requests_);
         !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue() != pending_request &&
          it.GetCurrentValue()->HasPermissionType(permission) &&
          it.GetCurrentValue()->requesting_origin == requesting_origin &&
          !it.GetCurrentValue()->IsCompleted(permission)) {
        should_not_cancel_ = true;
        break;
      }
    }
    if (should_not_cancel_)
      continue;

    switch (permission) {
      case PermissionType::GEOLOCATION:
        if (delegate)
          delegate->CancelGeolocationPermissionRequests(requesting_origin);
        break;
      case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
        if (delegate)
          delegate->CancelProtectedMediaIdentifierPermissionRequests(
              requesting_origin);
        break;
      case PermissionType::MIDI_SYSEX:
        if (delegate)
          delegate->CancelMIDISysexPermissionRequests(requesting_origin);
        break;
      case PermissionType::NOTIFICATIONS:
      case PermissionType::DURABLE_STORAGE:
      case PermissionType::AUDIO_CAPTURE:
      case PermissionType::VIDEO_CAPTURE:
      case PermissionType::BACKGROUND_SYNC:
      case PermissionType::FLASH:
      case PermissionType::ACCESSIBILITY_EVENTS:
      case PermissionType::CLIPBOARD_READ:
      case PermissionType::CLIPBOARD_WRITE:
      case PermissionType::PAYMENT_HANDLER:
      case PermissionType::BACKGROUND_FETCH:
      case PermissionType::IDLE_DETECTION:
      case PermissionType::PERIODIC_BACKGROUND_SYNC:
      case PermissionType::NFC:
        NOTIMPLEMENTED() << "CancelPermission not implemented for "
                         << static_cast<int>(permission);
        break;
      case PermissionType::MIDI:
      case PermissionType::SENSORS:
      case PermissionType::WAKE_LOCK_SCREEN:
      case PermissionType::WAKE_LOCK_SYSTEM:
        // There is nothing to cancel so this is simply ignored.
        break;
      case PermissionType::NUM:
        NOTREACHED() << "PermissionType::NUM was not expected here.";
        break;
    }
    pending_request->SetPermissionStatus(permission, PermissionStatus::DENIED);
  }

  // If there are still active requests, we should not remove request_id here,
  // but just do not invoke a relevant callback when the request is resolved in
  // OnRequestResponse().
  if (pending_request->IsCompleted())
    pending_requests_.Remove(request_id);
}

void AwPermissionManager::CancelPermissionRequests() {
  std::vector<int> request_ids;
  for (PendingRequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    request_ids.push_back(it.GetCurrentKey());
  }
  for (auto request_id : request_ids)
    CancelPermissionRequest(request_id);
  DCHECK(pending_requests_.IsEmpty());
}

int AwPermissionManager::GetRenderProcessID(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetProcess()->GetID();
}

int AwPermissionManager::GetRenderFrameID(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetRoutingID();
}

GURL AwPermissionManager::LastCommittedOrigin(
    content::RenderFrameHost* render_frame_host) {
  return content::WebContents::FromRenderFrameHost(render_frame_host)
      ->GetLastCommittedURL().GetOrigin();
}

AwBrowserPermissionRequestDelegate* AwPermissionManager::GetDelegate(
    int render_process_id, int render_frame_id) {
  return AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                    render_frame_id);
}

}  // namespace android_webview
