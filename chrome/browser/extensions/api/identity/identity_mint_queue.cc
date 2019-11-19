// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"

namespace extensions {

IdentityMintRequestQueue::IdentityMintRequestQueue() {
}

IdentityMintRequestQueue::~IdentityMintRequestQueue() {
  for (RequestQueueMap::const_iterator
           it = interactive_request_queue_map_.begin();
       it != interactive_request_queue_map_.end();
       ++it) {
    DCHECK_EQ(it->second.size(), 0lu);
  }
  for (RequestQueueMap::const_iterator
           it = noninteractive_request_queue_map_.begin();
       it != noninteractive_request_queue_map_.end();
       ++it) {
    DCHECK_EQ(it->second.size(), 0lu);
  }
}

void IdentityMintRequestQueue::RequestStart(
    IdentityMintRequestQueue::MintType type,
    const ExtensionTokenKey& key,
    IdentityMintRequestQueue::Request* request) {
  TRACE_EVENT_ASYNC_BEGIN1(
      "identity", "IdentityMintRequestQueue", request, "type", type);
  RequestQueue& request_queue = GetRequestQueueMap(type)[key];
  request_queue.push_back(request);
  // If this is the first request, start it now. RequestComplete will start
  // all other requests.
  if (request_queue.size() == 1)
    RunRequest(type, request_queue);
}

void IdentityMintRequestQueue::RequestComplete(
    IdentityMintRequestQueue::MintType type,
    const ExtensionTokenKey& key,
    IdentityMintRequestQueue::Request* request) {
  TRACE_EVENT_ASYNC_END1("identity",
                         "IdentityMintRequestQueue",
                         request,
                         "completed",
                         "RequestComplete");
  RequestQueue& request_queue = GetRequestQueueMap(type)[key];
  CHECK_EQ(request_queue.front(), request);
  request_queue.pop_front();
  if (!request_queue.empty())
    RunRequest(type, request_queue);
}

void IdentityMintRequestQueue::RequestCancel(
    const ExtensionTokenKey& key,
    IdentityMintRequestQueue::Request* request) {
  TRACE_EVENT_ASYNC_END1("identity",
                         "IdentityMintRequestQueue",
                         request,
                         "completed",
                         "RequestCancel");
  GetRequestQueueMap(MINT_TYPE_INTERACTIVE)[key].remove(request);
  GetRequestQueueMap(MINT_TYPE_NONINTERACTIVE)[key].remove(request);
}

bool IdentityMintRequestQueue::empty(IdentityMintRequestQueue::MintType type,
                                     const ExtensionTokenKey& key) {
  RequestQueueMap& request_queue_map = GetRequestQueueMap(type);
  return !base::Contains(request_queue_map, key) ||
         (request_queue_map.find(key))->second.empty();
}

IdentityMintRequestQueue::RequestQueueMap&
IdentityMintRequestQueue::GetRequestQueueMap(
    IdentityMintRequestQueue::MintType type) {
  return (type == MINT_TYPE_INTERACTIVE) ? interactive_request_queue_map_
                                         : noninteractive_request_queue_map_;
}

void IdentityMintRequestQueue::RunRequest(
    IdentityMintRequestQueue::MintType type,
    RequestQueue& request_queue) {
  TRACE_EVENT_ASYNC_STEP_INTO0(
      "identity", "IdentityMintRequestQueue", request_queue.front(), "RUNNING");
  request_queue.front()->StartMintToken(type);
}

}  // namespace extensions
