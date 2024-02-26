// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_MINT_QUEUE_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_MINT_QUEUE_H_

#include <list>
#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"

namespace extensions {

// getAuthToken requests are serialized to avoid excessive traffic to
// GAIA and to consolidate UI pop-ups. IdentityMintRequestQueue
// maitains a set of queues, one for each RequestKey.
//
// The queue calls StartMintToken on each Request when it reaches the
// head of the line.
//
// The queue does not own Requests. Request pointers must be valid
// until they are removed from the queue with RequestComplete or
// RequestCancel.
class IdentityMintRequestQueue {
 public:
  enum MintType {
    MINT_TYPE_NONINTERACTIVE,
    MINT_TYPE_INTERACTIVE
  };

  IdentityMintRequestQueue();
  virtual ~IdentityMintRequestQueue();

  class Request {
   public:
    virtual ~Request() {}
    virtual void StartMintToken(IdentityMintRequestQueue::MintType type) = 0;
  };

  // Adds a request to the queue specified by the token key.
  void RequestStart(IdentityMintRequestQueue::MintType type,
                    const ExtensionTokenKey& key,
                    IdentityMintRequestQueue::Request* request);
  // Removes a request from the queue specified by the token key.
  void RequestComplete(IdentityMintRequestQueue::MintType type,
                       const ExtensionTokenKey& key,
                       IdentityMintRequestQueue::Request* request);
  // Cancels a request. OK to call if |request| is not queued.
  // Does *not* start a new request, even if the canceled request is at
  // the head of the queue.
  void RequestCancel(const ExtensionTokenKey& key,
                     IdentityMintRequestQueue::Request* request);
  bool empty(IdentityMintRequestQueue::MintType type,
             const ExtensionTokenKey& key);

 private:
  typedef std::list<raw_ptr<IdentityMintRequestQueue::Request, CtnExperimental>>
      RequestQueue;
  typedef std::map<const ExtensionTokenKey, RequestQueue> RequestQueueMap;

  RequestQueueMap& GetRequestQueueMap(IdentityMintRequestQueue::MintType type);
  void RunRequest(IdentityMintRequestQueue::MintType type,
                  RequestQueue& request_queue);

  RequestQueueMap interactive_request_queue_map_;
  RequestQueueMap noninteractive_request_queue_map_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_MINT_QUEUE_H_
