// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_REQUEST_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_REQUEST_HANDLER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace android_webview {

class AwPermissionRequest;
class AwPermissionRequestDelegate;
class PermissionRequestHandlerClient;

// This class is used to send the permission requests, or cancel ongoing
// requests.
// It is owned by AwContents and has 1x1 mapping to AwContents. All methods
// are running on UI thread.
class PermissionRequestHandler : public content::WebContentsObserver {
 public:
  PermissionRequestHandler(PermissionRequestHandlerClient* client,
                           content::WebContents* aw_contents);
  ~PermissionRequestHandler() override;

  // Send the given |request| to PermissionRequestHandlerClient.
  void SendRequest(std::unique_ptr<AwPermissionRequestDelegate> request);

  // Cancel the ongoing request initiated by |origin| for accessing |resources|.
  void CancelRequest(const GURL& origin, int64_t resources);

  // Allow |origin| to access the |resources|.
  void PreauthorizePermission(const GURL& origin, int64_t resources);

  // WebContentsObserver
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

 private:
  friend class TestPermissionRequestHandler;

  typedef std::vector<base::WeakPtr<AwPermissionRequest>>::iterator
      RequestIterator;

  // Return the request initiated by |origin| for accessing |resources|.
  RequestIterator FindRequest(const GURL& origin, int64_t resources);

  // Cancel the given request.
  void CancelRequestInternal(RequestIterator i);

  void CancelAllRequests();

  // Remove the invalid requests from requests_.
  void PruneRequests();

  // Return true if |origin| were preauthorized to access |resources|.
  bool Preauthorized(const GURL& origin, int64_t resources);

  PermissionRequestHandlerClient* client_;

  // A list of ongoing requests.
  std::vector<base::WeakPtr<AwPermissionRequest>> requests_;

  std::map<std::string, int64_t> preauthorized_permission_;

  // The unique id of the active NavigationEntry of the WebContents that we were
  // opened for. Used to help expire on requests.
  int contents_unique_id_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestHandler);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PERMISSION_PERMISSION_REQUEST_HANDLER_H_
