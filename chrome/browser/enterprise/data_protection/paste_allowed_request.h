// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PASTE_ALLOWED_REQUEST_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PASTE_ALLOWED_REQUEST_H_

#include <map>
#include <optional>

#include "base/containers/flat_map.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"

namespace enterprise_data_protection {

// Keeps track of a request to see if some clipboard content, identified by
// its sequence number, is allowed to be pasted into the RenderFrameHost
// that owns this clipboard host.
//
// A request starts in the state incomplete until Complete() is called with
// a value.  Callbacks can be added to the request before or after it has
// completed.
class PasteAllowedRequest {
 public:
  using IsClipboardPasteAllowedCallback =
      content::ContentBrowserClient::IsClipboardPasteAllowedCallback;

  using RequestsMap =
      std::map<content::GlobalRenderFrameHostId,
               std::map<ui::ClipboardSequenceNumberToken, PasteAllowedRequest>>;

  // A paste allowed request is obsolete if it is older than this time.
  static const base::TimeDelta kIsPasteAllowedRequestTooOld;

  // Called by PerformPasteIfAllowed() when an is allowed request is
  // needed.
  static void StartPasteAllowedRequest(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination,
      const content::ClipboardMetadata& metadata,
      content::ClipboardPasteData clipboard_paste_data,
      IsClipboardPasteAllowedCallback callback);

  static size_t requests_count_for_testing();

  // Remove obsolete entries from the outstanding requests map.
  // A request is obsolete if:
  //  - its sequence number is less than |seqno|
  //  - it has no callbacks
  //  - it is too old
  static void CleanupObsoleteRequests();

  // Removes all requests, even if they are not yet obsolete.
  static void CleanupRequestsForTesting();

  static void AddRequestToCacheForTesting(
      content::GlobalRenderFrameHostId rfh_id,
      ui::ClipboardSequenceNumberToken seqno,
      PasteAllowedRequest request);

  PasteAllowedRequest();
  PasteAllowedRequest(PasteAllowedRequest&&);
  PasteAllowedRequest& operator=(PasteAllowedRequest&&);
  ~PasteAllowedRequest();

  // Adds `callback` to be notified when the request completes. Returns true
  // if this is the first callback added and a request should be started,
  // returns false otherwise.
  bool AddCallback(IsClipboardPasteAllowedCallback callback);

  // Merge `data` into the existing internal `data_` member so that the
  // currently pending request will have the appropriate fields for all added
  // callbacks, not just the initial one that created the request.
  void AddData(content::ClipboardPasteData data);

  // Mark this request as completed with the specified result.
  // Invoke all callbacks now.
  void Complete(std::optional<content::ClipboardPasteData> data);

  // Invokes `callback` immediately for a completed request as no
  // asynchronous work is required to check if `data` is allowed to be pasted.
  // Should only be invoked is `is_complete()` returns true.
  void InvokeCallback(content::ClipboardPasteData data,
                      IsClipboardPasteAllowedCallback callback);

  // Returns true if the request has completed.
  bool is_complete() const { return data_allowed_.has_value(); }

  // Returns true if this request is obsolete.  An obsolete request
  // is one that is completed, all registered callbacks have been
  // called, and is considered old.
  //
  // |now| represents the current time.  It is an argument to ease testing.
  bool IsObsolete(base::Time now);

  // Returns the time at which this request was completed.  If called
  // before the request is completed the return value is undefined.
  base::Time completed_time() const;

 private:
  // Calls all the callbacks in |callbacks_| with the current value of
  // |allowed_|.  |allowed_| must not be empty.
  void InvokeCallbacks();

  // The time at which the request was completed.  Before completion this
  // value is undefined.
  base::Time completed_time_;

  // This member is null until Complete() is called.
  std::optional<bool> data_allowed_;

  // The data argument to pass to the IsClipboardPasteAllowedCallback.
  content::ClipboardPasteData data_;
  std::vector<IsClipboardPasteAllowedCallback> callbacks_;
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_PASTE_ALLOWED_REQUEST_H_
