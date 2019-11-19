// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/request_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Timeout in seconds, before a request is considered as stale and hence
// aborted.
const int kDefaultTimeout = 10;

}  // namespace

RequestManager::RequestManager(
    Profile* profile,
    const std::string& provider_id,
    NotificationManagerInterface* notification_manager)
    : profile_(profile),
      provider_id_(provider_id),
      notification_manager_(notification_manager),
      next_id_(1),
      timeout_(base::TimeDelta::FromSeconds(kDefaultTimeout)) {}

RequestManager::~RequestManager() {
  // Abort all of the active requests.
  auto it = requests_.begin();
  while (it != requests_.end()) {
    const int request_id = it->first;
    ++it;
    RejectRequest(request_id, std::make_unique<RequestValue>(),
                  base::File::FILE_ERROR_ABORT);
  }

  DCHECK_EQ(0u, requests_.size());
}

int RequestManager::CreateRequest(RequestType type,
                                  std::unique_ptr<HandlerInterface> handler) {
  // The request id is unique per request manager, so per service, thereof
  // per profile.
  int request_id = next_id_++;

  // If cycled the int, then signal an error.
  if (requests_.find(request_id) != requests_.end())
    return 0;

  TRACE_EVENT_ASYNC_BEGIN1("file_system_provider",
                           "RequestManager::Request",
                           request_id,
                           "type",
                           type);

  std::unique_ptr<Request> request = std::make_unique<Request>();
  request->handler = std::move(handler);
  requests_[request_id] = std::move(request);
  ResetTimer(request_id);

  for (auto& observer : observers_)
    observer.OnRequestCreated(request_id, type);

  // Execute the request implementation. In case of an execution failure,
  // unregister and return 0. This may often happen, eg. if the providing
  // extension is not listening for the request event being sent.
  // In such case, we should abort as soon as possible.
  if (!requests_[request_id]->handler->Execute(request_id)) {
    DestroyRequest(request_id);
    return 0;
  }

  for (auto& observer : observers_)
    observer.OnRequestExecuted(request_id);

  return request_id;
}

base::File::Error RequestManager::FulfillRequest(
    int request_id,
    std::unique_ptr<RequestValue> response,
    bool has_more) {
  CHECK(response.get());
  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return base::File::FILE_ERROR_NOT_FOUND;

  for (auto& observer : observers_)
    observer.OnRequestFulfilled(request_id, *response.get(), has_more);

  request_it->second->handler->OnSuccess(request_id, std::move(response),
                                         has_more);

  if (!has_more) {
    DestroyRequest(request_id);
  } else {
    if (notification_manager_)
      notification_manager_->HideUnresponsiveNotification(request_id);
    ResetTimer(request_id);
  }

  return base::File::FILE_OK;
}

base::File::Error RequestManager::RejectRequest(
    int request_id,
    std::unique_ptr<RequestValue> response,
    base::File::Error error) {
  CHECK(response.get());
  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return base::File::FILE_ERROR_NOT_FOUND;

  for (auto& observer : observers_)
    observer.OnRequestRejected(request_id, *response.get(), error);
  request_it->second->handler->OnError(request_id, std::move(response), error);
  DestroyRequest(request_id);

  return base::File::FILE_OK;
}

void RequestManager::SetTimeoutForTesting(const base::TimeDelta& timeout) {
  timeout_ = timeout;
}

std::vector<int> RequestManager::GetActiveRequestIds() const {
  std::vector<int> result;

  for (auto request_it = requests_.begin(); request_it != requests_.end();
       ++request_it) {
    result.push_back(request_it->first);
  }

  return result;
}

void RequestManager::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RequestManager::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

RequestManager::Request::Request() {}

RequestManager::Request::~Request() {}

void RequestManager::OnRequestTimeout(int request_id) {
  for (auto& observer : observers_)
    observer.OnRequestTimeouted(request_id);

  if (!notification_manager_) {
    RejectRequest(request_id, std::make_unique<RequestValue>(),
                  base::File::FILE_ERROR_ABORT);
    return;
  }

  if (!IsInteractingWithUser()) {
    notification_manager_->ShowUnresponsiveNotification(
        request_id,
        base::Bind(&RequestManager::OnUnresponsiveNotificationResult,
                   weak_ptr_factory_.GetWeakPtr(), request_id));
  } else {
    ResetTimer(request_id);
  }
}

void RequestManager::OnUnresponsiveNotificationResult(
    int request_id,
    NotificationManagerInterface::NotificationResult result) {
  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  if (result == NotificationManagerInterface::CONTINUE) {
    ResetTimer(request_id);
    return;
  }

  RejectRequest(request_id, std::make_unique<RequestValue>(),
                base::File::FILE_ERROR_ABORT);
}

void RequestManager::ResetTimer(int request_id) {
  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  request_it->second->timeout_timer.Start(
      FROM_HERE,
      timeout_,
      base::Bind(&RequestManager::OnRequestTimeout,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id));
}

bool RequestManager::IsInteractingWithUser() const {
  // First try for app windows. If not found, then fall back to browser windows
  // and tabs.

  const extensions::AppWindowRegistry* const registry =
      extensions::AppWindowRegistry::Get(profile_);
  DCHECK(registry);
  if (registry->GetCurrentAppWindowForApp(provider_id_))
    return true;

  // This loop is heavy, but it's not called often. Only when a request timeouts
  // which is at most once every 10 seconds per request (except tests).
  const extensions::WindowControllerList::ControllerList& windows =
      extensions::WindowControllerList::GetInstance()->windows();
  for (auto* window : windows) {
    const Browser* const browser = window->GetBrowser();
    if (!browser)
      continue;
    const TabStripModel* const tabs = browser->tab_strip_model();
    DCHECK(tabs);
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* const web_contents = tabs->GetWebContentsAt(i);
      const GURL& url = web_contents->GetURL();
      if (url.SchemeIs(extensions::kExtensionScheme) &&
          url.host_piece() == provider_id_) {
        return true;
      }
    }
  }

  return false;
}

void RequestManager::DestroyRequest(int request_id) {
  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  requests_.erase(request_it);

  if (notification_manager_)
    notification_manager_->HideUnresponsiveNotification(request_id);

  for (auto& observer : observers_)
    observer.OnRequestDestroyed(request_id);

  TRACE_EVENT_ASYNC_END0(
      "file_system_provider", "RequestManager::Request", request_id);
}

}  // namespace file_system_provider
}  // namespace chromeos
