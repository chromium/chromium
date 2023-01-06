// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/permission/permission_request_handler.h"

#include <utility>

#include "android_webview/browser/permission/aw_permission_request.h"
#include "android_webview/browser/permission/aw_permission_request_delegate.h"
#include "android_webview/browser/permission/permission_request_handler_client.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {

int GetLastCommittedEntryID(content::WebContents* web_contents) {
  if (!web_contents)
    return 0;

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  return entry ? entry->GetUniqueID() : 0;
}

}  // namespace

PermissionRequestHandler::PermissionRequestHandler(
    PermissionRequestHandlerClient* client,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      client_(client),
      contents_unique_id_(GetLastCommittedEntryID(web_contents)) {}

PermissionRequestHandler::~PermissionRequestHandler() {
  CancelAllRequests();
}

void PermissionRequestHandler::SendRequest(
    std::unique_ptr<AwPermissionRequestDelegate> request) {
  if (Preauthorized(request->GetOrigin(), request->GetResources())) {
    request->NotifyRequestResult(true);
    return;
  }

  base::WeakPtr<AwPermissionRequest> weak_request;
  base::android::ScopedJavaLocalRef<jobject> java_peer =
      AwPermissionRequest::Create(std::move(request), &weak_request);
  requests_.push_back(weak_request);
  client_->OnPermissionRequest(java_peer, weak_request.get());
  PruneRequests();
}

void PermissionRequestHandler::CancelRequest(const GURL& origin,
                                             int64_t resources) {
  // The request list might have multiple requests with same origin and
  // resources.
  RequestIterator i = FindRequest(origin, resources);
  while (i != requests_.end()) {
    CancelRequestInternal(i);
    requests_.erase(i);
    i = FindRequest(origin, resources);
  }
}

void PermissionRequestHandler::PreauthorizePermission(const GURL& origin,
                                                      int64_t resources) {
  if (!resources)
    return;

  std::string key = origin.DeprecatedGetOriginAsURL().spec();
  if (key.empty()) {
    LOG(ERROR) << "The origin of preauthorization is empty, ignore it.";
    return;
  }

  preauthorized_permission_[key] |= resources;
}

void PermissionRequestHandler::NavigationEntryCommitted(
    const content::LoadCommittedDetails& details) {
  const ui::PageTransition transition = details.entry->GetTransitionType();
  if (details.is_navigation_to_different_page() ||
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) ||
      contents_unique_id_ != details.entry->GetUniqueID()) {
    CancelAllRequests();
    contents_unique_id_ = details.entry->GetUniqueID();
  }
}

PermissionRequestHandler::RequestIterator PermissionRequestHandler::FindRequest(
    const GURL& origin,
    int64_t resources) {
  RequestIterator i;
  for (i = requests_.begin(); i != requests_.end(); ++i) {
    if (i->get() && i->get()->GetOrigin() == origin &&
        i->get()->GetResources() == resources) {
      break;
    }
  }
  return i;
}

void PermissionRequestHandler::CancelRequestInternal(RequestIterator i) {
  AwPermissionRequest* request = i->get();
  if (request) {
    client_->OnPermissionRequestCanceled(request);
    request->CancelAndDelete();
  }
}

void PermissionRequestHandler::CancelAllRequests() {
  for (RequestIterator i = requests_.begin(); i != requests_.end(); ++i)
    CancelRequestInternal(i);
}

void PermissionRequestHandler::PruneRequests() {
  for (RequestIterator i = requests_.begin(); i != requests_.end();) {
    if (!i->get())
      i = requests_.erase(i);
    else
      ++i;
  }
}

bool PermissionRequestHandler::Preauthorized(const GURL& origin,
                                             int64_t resources) {
  std::map<std::string, int64_t>::iterator i =
      preauthorized_permission_.find(origin.DeprecatedGetOriginAsURL().spec());

  return i != preauthorized_permission_.end() &&
         (resources & i->second) == resources;
}

}  // namespace android_webview
