// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_permission_manager.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "android_webview/browser/aw_app_defined_websites.h"
#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_context_permissions_delegate.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/common/aw_features.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

using blink::PermissionType;
using blink::mojom::PermissionStatus;

using RequestPermissionsCallback =
    base::OnceCallback<void(const std::vector<PermissionStatus>&)>;

namespace android_webview {

class LastRequestResultCache {
 public:
  LastRequestResultCache() = default;

  LastRequestResultCache(const LastRequestResultCache&) = delete;
  LastRequestResultCache& operator=(const LastRequestResultCache&) = delete;

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
    }
    if (!embedding_origin.is_valid()) {
      NOTREACHED() << embedding_origin.possibly_invalid_spec();
    }

    switch (permission) {
      case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      case PermissionType::STORAGE_ACCESS_GRANT:
      case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
        break;
      // Other permissions are not cached.
      default:
        return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    CHECK(!key.empty());
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

    switch (permission) {
      case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      case PermissionType::STORAGE_ACCESS_GRANT:
      case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
        break;
      // Other permissions are not cached.
      default:
        NOTREACHED()
            << "Results are only cached for PROTECTED_MEDIA_IDENTIFIER AND SAA";
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
};

class AwPermissionManager::PendingRequest {
 public:
  PendingRequest(const std::vector<PermissionType>& permissions,
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
    }
    DCHECK(!IsCompleted());
    results[result->second] = status;
    if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
      if (type == PermissionType::MIDI && status == PermissionStatus::GRANTED) {
        content::ChildProcessSecurityPolicy::GetInstance()
            ->GrantSendMidiMessage(render_process_id);
      }
    }
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
    }
    return results[result->second];
  }

  bool HasPermissionType(PermissionType type) {
    return base::Contains(permission_index_map_, type);
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

// Regarding the saa_cache_ size: 99% of WebView site visits fall
// into this count (based on Android.WebView.SitesVisitedWeekly) in a week so we
// will cache those for revisits but not going above it to avoid using up too
// much memory for any heavy case. This will cache in memory so every app reload
// will require a new request. This will also be duplicated across profiles
// which may be a useful property in the future so we aren't going to design
// around that.
AwPermissionManager::AwPermissionManager(
    const AwContextPermissionsDelegate& context_delegate)
    : context_delegate_(context_delegate),
      result_cache_(new LastRequestResultCache),
      saa_cache_(10) {}

AwPermissionManager::~AwPermissionManager() {
  CancelPermissionRequests();
}

void AwPermissionManager::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto const& permissions = request_description.permissions;
  if (permissions.empty()) {
    std::move(callback).Run(std::vector<PermissionStatus>());
    return;
  }

  const GURL& embedding_origin = LastCommittedMainOrigin(render_frame_host);
  const GURL& requesting_origin = request_description.requesting_origin;

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
      case PermissionType::CLIPBOARD_SANITIZED_WRITE:
        // This is the permission for writing vetted data (such as plain text or
        // sanitized images) using the async clipboard API.
        // This permission type implies that user gesture is present, and as
        // such, it can be auto-granted to conform with Chrome logic.
        // Reading from the clipboard or writing
        // custom data is represented with the CLIPBOARD_READ_WRITE permission,
        // and that requires an explicit user approval, which is not implemented
        // yet. See crbug.com/1271620
        if (base::FeatureList::IsEnabled(
                features::kWebViewAutoGrantSanitizedClipboardWrite)) {
          pending_request_raw->SetPermissionStatus(permissions[i],
                                                   PermissionStatus::GRANTED);
        } else {
          pending_request_raw->SetPermissionStatus(
              permissions[i], request_description.user_gesture
                                  ? PermissionStatus::GRANTED
                                  : PermissionStatus::DENIED);
        }
        break;
      case PermissionType::AUDIO_CAPTURE:
      case PermissionType::VIDEO_CAPTURE:
      case PermissionType::NOTIFICATIONS:
      case PermissionType::DURABLE_STORAGE:
      case PermissionType::BACKGROUND_SYNC:
      case PermissionType::CLIPBOARD_READ_WRITE:
      case PermissionType::PAYMENT_HANDLER:
      case PermissionType::BACKGROUND_FETCH:
      case PermissionType::IDLE_DETECTION:
      case PermissionType::PERIODIC_BACKGROUND_SYNC:
      case PermissionType::NFC:
      case PermissionType::VR:
      case PermissionType::AR:
      case PermissionType::HAND_TRACKING:
      case PermissionType::CAMERA_PAN_TILT_ZOOM:
      case PermissionType::WINDOW_MANAGEMENT:
      case PermissionType::LOCAL_FONTS:
      case PermissionType::DISPLAY_CAPTURE:
      case PermissionType::CAPTURED_SURFACE_CONTROL:
      case PermissionType::SMART_CARD:
      case PermissionType::WEB_PRINTING:
      case PermissionType::SPEAKER_SELECTION:
      case PermissionType::KEYBOARD_LOCK:
      case PermissionType::POINTER_LOCK:
      case PermissionType::AUTOMATIC_FULLSCREEN:
      case PermissionType::WEB_APP_INSTALLATION:
        NOTIMPLEMENTED() << "RequestPermissions is not implemented for "
                         << static_cast<int>(permissions[i]);
        pending_request_raw->SetPermissionStatus(permissions[i],
                                                 PermissionStatus::DENIED);
        break;
      case PermissionType::STORAGE_ACCESS_GRANT:
      case PermissionType::TOP_LEVEL_STORAGE_ACCESS: {
        const url::Origin& outer_origin =
            render_frame_host->GetOutermostMainFrame()
                ->GetLastCommittedOrigin();

        auto cached_value = saa_cache_->Get(outer_origin.Serialize());
        if (cached_value != saa_cache_->end()) {
          auto is_granted = cached_value->second ? PermissionStatus::GRANTED
                                                 : PermissionStatus::DENIED;
          pending_request_raw->SetPermissionStatus(permissions[i], is_granted);
          break;
        }

        auto on_saa_response =
            base::BindOnce(&CacheAutoSAA, weak_ptr_factory_.GetWeakPtr(),
                           outer_origin)
                .Then(base::BindOnce(&OnRequestResponse,
                                     weak_ptr_factory_.GetWeakPtr(), request_id,
                                     permissions[i]));

        delegate->RequestStorageAccess(outer_origin,
                                       std::move(on_saa_response));
        break;
      }
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
    }
  }

  // If delegate resolve the permission synchronously, all requests could be
  // already resolved here.
  if (!pending_requests_.Lookup(request_id))
    return;

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
  }
}

// static
bool AwPermissionManager::CacheAutoSAA(
    const base::WeakPtr<AwPermissionManager>& manager,
    const url::Origin& origin,
    bool allowed) {
  // All delegate functions should be cancelled when the manager runs
  // destructor. Therefore |manager| should be always valid here.
  CHECK(manager);
  DCHECK_CALLED_ON_VALID_SEQUENCE(manager->sequence_checker_);

  DVLOG(1) << "Caching auto granted SAA result " << allowed << " for origin "
           << origin;

  manager->saa_cache_->Put({
      origin.Serialize(),
      allowed,
  });

  return allowed;
}

// static
void AwPermissionManager::OnRequestResponse(
    const base::WeakPtr<AwPermissionManager>& manager,
    int request_id,
    PermissionType permission,
    bool allowed) {
  // All delegate functions should be cancelled when the manager runs
  // destructor. Therefore |manager| should be always valid here.
  CHECK(manager);

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

void AwPermissionManager::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  RequestPermissions(render_frame_host, request_description,
                     std::move(callback));
}

PermissionStatus AwPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return GetPermissionStatusInternal(permission, requesting_origin,
                                     embedding_origin,
                                     /*web_contents=*/nullptr);
}

PermissionStatus AwPermissionManager::GetPermissionStatusInternal(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    content::WebContents* web_contents) {
  switch (permission) {
    // Setting results is called outside the Permissions API only for these
    // permissions.
    case blink::PermissionType::STORAGE_ACCESS_GRANT:
    case blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS: {
      if (!base::FeatureList::IsEnabled(features::kWebViewAutoSAA)) {
        return PermissionStatus::DENIED;
      }
      return result_cache_->GetResult(permission, requesting_origin,
                                      embedding_origin);
    }
    case blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return result_cache_->GetResult(permission, requesting_origin,
                                      embedding_origin);

    case blink::PermissionType::GEOLOCATION:
      return GetGeolocationPermission(requesting_origin, web_contents);

    case blink::PermissionType::CLIPBOARD_SANITIZED_WRITE:
      // These permissions are auto-granted by WebView.
      if (base::FeatureList::IsEnabled(
              features::kWebViewAutoGrantSanitizedClipboardWrite)) {
        return PermissionStatus::GRANTED;
      } else {
        return PermissionStatus::ASK;
      }

    case blink::PermissionType::MIDI:
    case blink::PermissionType::SENSORS:
      // These permissions are auto-granted by WebView.
      return PermissionStatus::GRANTED;

    case blink::PermissionType::MIDI_SYSEX:
    case blink::PermissionType::AUDIO_CAPTURE:
    case blink::PermissionType::VIDEO_CAPTURE:
      // These permissions are always forwarded to the app to handle.
      return PermissionStatus::ASK;

    case blink::PermissionType::AR:
    case blink::PermissionType::AUTOMATIC_FULLSCREEN:
    case blink::PermissionType::BACKGROUND_FETCH:
    case blink::PermissionType::BACKGROUND_SYNC:
    case blink::PermissionType::CAMERA_PAN_TILT_ZOOM:
    case blink::PermissionType::CAPTURED_SURFACE_CONTROL:
    case blink::PermissionType::CLIPBOARD_READ_WRITE:
    case blink::PermissionType::DISPLAY_CAPTURE:
    case blink::PermissionType::DURABLE_STORAGE:
    case blink::PermissionType::HAND_TRACKING:
    case blink::PermissionType::IDLE_DETECTION:
    case blink::PermissionType::KEYBOARD_LOCK:
    case blink::PermissionType::LOCAL_FONTS:
    case blink::PermissionType::NFC:
    case blink::PermissionType::NOTIFICATIONS:
    case blink::PermissionType::NUM:
    case blink::PermissionType::PAYMENT_HANDLER:
    case blink::PermissionType::PERIODIC_BACKGROUND_SYNC:
    case blink::PermissionType::POINTER_LOCK:
    case blink::PermissionType::SMART_CARD:
    case blink::PermissionType::SPEAKER_SELECTION:
    case blink::PermissionType::VR:
    case blink::PermissionType::WAKE_LOCK_SCREEN:
    case blink::PermissionType::WAKE_LOCK_SYSTEM:
    case blink::PermissionType::WEB_APP_INSTALLATION:
    case blink::PermissionType::WEB_PRINTING:
    case blink::PermissionType::WINDOW_MANAGEMENT:
      return PermissionStatus::DENIED;
  }
  NOTREACHED() << "Unhandled permission type: " << static_cast<int>(permission);
}

PermissionStatus AwPermissionManager::GetGeolocationPermission(
    const GURL& requesting_origin,
    content::WebContents* web_contents) {
  if (!web_contents) {
    // If we don't have a web_contents, we can't determine if we have
    // permission.
    return PermissionStatus::ASK;
  }

  AwSettings* settings = AwSettings::FromWebContents(web_contents);
  if (!settings->geolocation_enabled()) {
    return PermissionStatus::DENIED;
  }
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents->UseLegacyGeolocationPermissionAPI()) {
    // The new geolocation API does not have a cache for permission decisions,
    // so if that's in use, we will need to ask the app.
    return PermissionStatus::ASK;
  }
  return context_delegate_->GetGeolocationPermission(requesting_origin);
}

content::PermissionResult
AwPermissionManager::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status = GetPermissionStatus(
      permission, requesting_origin.GetURL(), embedding_origin.GetURL());

  return content::PermissionResult(
      status, content::PermissionStatusSource::UNSPECIFIED);
}

PermissionStatus AwPermissionManager::GetPermissionStatusForCurrentDocument(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  return GetPermissionStatusInternal(
      permission,
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host),
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()),
      web_contents);
}

PermissionStatus AwPermissionManager::GetPermissionStatusForWorker(
    PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return GetPermissionStatus(permission, worker_origin, worker_origin);
}

PermissionStatus AwPermissionManager::GetPermissionStatusForEmbeddedRequester(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& requesting_origin) {
  return GetPermissionStatusInternal(
      permission, requesting_origin.GetURL(),
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()),
      content::WebContents::FromRenderFrameHost(render_frame_host));
}

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
      case PermissionType::CLIPBOARD_READ_WRITE:
      case PermissionType::CLIPBOARD_SANITIZED_WRITE:
      case PermissionType::PAYMENT_HANDLER:
      case PermissionType::BACKGROUND_FETCH:
      case PermissionType::IDLE_DETECTION:
      case PermissionType::PERIODIC_BACKGROUND_SYNC:
      case PermissionType::NFC:
      case PermissionType::HAND_TRACKING:
      case PermissionType::VR:
      case PermissionType::AR:
      case PermissionType::STORAGE_ACCESS_GRANT:
      case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      case PermissionType::CAMERA_PAN_TILT_ZOOM:
      case PermissionType::WINDOW_MANAGEMENT:
      case PermissionType::LOCAL_FONTS:
      case PermissionType::DISPLAY_CAPTURE:
      case PermissionType::CAPTURED_SURFACE_CONTROL:
      case PermissionType::SMART_CARD:
      case PermissionType::WEB_PRINTING:
      case PermissionType::SPEAKER_SELECTION:
      case PermissionType::KEYBOARD_LOCK:
      case PermissionType::POINTER_LOCK:
      case PermissionType::AUTOMATIC_FULLSCREEN:
      case PermissionType::WEB_APP_INSTALLATION:
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

void AwPermissionManager::SetOriginCanReadEnumerateDevicesAudioLabels(
    const url::Origin& origin,
    bool audio) {
  auto it = enumerate_devices_labels_cache_.find(origin);
  if (it == enumerate_devices_labels_cache_.end()) {
    enumerate_devices_labels_cache_[origin] = std::make_pair(audio, false);
  } else {
    it->second.first = audio;
  }
}

void AwPermissionManager::SetOriginCanReadEnumerateDevicesVideoLabels(
    const url::Origin& origin,
    bool video) {
  auto it = enumerate_devices_labels_cache_.find(origin);
  if (it == enumerate_devices_labels_cache_.end())
    enumerate_devices_labels_cache_[origin] = std::make_pair(false, video);
  else
    it->second.second = video;
}

bool AwPermissionManager::ShouldShowEnumerateDevicesAudioLabels(
    const url::Origin& origin) {
  auto it = enumerate_devices_labels_cache_.find(origin);
  if (it == enumerate_devices_labels_cache_.end())
    return false;
  return it->second.first;
}

bool AwPermissionManager::ShouldShowEnumerateDevicesVideoLabels(
    const url::Origin& origin) {
  auto it = enumerate_devices_labels_cache_.find(origin);
  if (it == enumerate_devices_labels_cache_.end())
    return false;
  return it->second.second;
}

void AwPermissionManager::ClearEnumerateDevicesCachedPermission(
    const url::Origin& origin,
    bool remove_audio,
    bool remove_video) {
  auto it = enumerate_devices_labels_cache_.find(origin);
  if (it == enumerate_devices_labels_cache_.end())
    return;
  else if (remove_audio && remove_video) {
    enumerate_devices_labels_cache_.erase(origin);
  } else if (remove_audio) {
    it->second.first = false;
  } else if (remove_video) {
    it->second.second = false;
  }
}

int AwPermissionManager::GetRenderProcessID(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetProcess()->GetID();
}

int AwPermissionManager::GetRenderFrameID(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetRoutingID();
}

GURL AwPermissionManager::LastCommittedMainOrigin(
    content::RenderFrameHost* render_frame_host) {
  return permissions::PermissionUtil::GetLastCommittedOriginAsURL(
      render_frame_host->GetMainFrame());
}

AwBrowserPermissionRequestDelegate* AwPermissionManager::GetDelegate(
    int render_process_id,
    int render_frame_id) {
  return AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                    render_frame_id);
}

}  // namespace android_webview
