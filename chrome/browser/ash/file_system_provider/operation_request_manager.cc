// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"

#include "base/time/time.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"

namespace ash::file_system_provider {
namespace {

}  // namespace

OperationRequestManager::OperationRequestManager(
    Profile* profile,
    const std::string& provider_id,
    NotificationManagerInterface* notification_manager,
    base::TimeDelta timeout)
    : RequestManager(profile, notification_manager, timeout),
      provider_id_(provider_id) {}

OperationRequestManager::~OperationRequestManager() = default;

void OperationRequestManager::OnRequestTimeout(int request_id) {
  for (auto& observer : observers_)
    observer.OnRequestTimedOut(request_id);

  if (!notification_manager_) {
    RejectRequestInternal(request_id, RequestValue(),
                          base::File::FILE_ERROR_ABORT,
                          OperationCompletion::kAbortedInternally);
    return;
  }

  auto request_it = requests_.find(request_id);
  if (request_it == requests_.end()) {
    return;
  }

  if (!IsInteractingWithUser()) {
    request_it->second->shown_unresponsive_notification = true;
    notification_manager_->ShowUnresponsiveNotification(
        request_id,
        base::BindOnce(
            &OperationRequestManager::OnUnresponsiveNotificationResult,
            weak_ptr_factory_.GetWeakPtr(), request_id));
  } else {
    ResetTimer(request_id);
  }
}

bool OperationRequestManager::IsInteractingWithUser() const {
  if (current_user_interactions_ > 0) {
    return true;
  }

  // First try for app windows. If not found, then fall back to browser windows
  // and tabs.
  const extensions::AppWindowRegistry* const registry =
      extensions::AppWindowRegistry::Get(profile_);
  DCHECK(registry);
  if (registry->GetCurrentAppWindowForApp(provider_id_))
    return true;

  // This loop is heavy, but it's not called often. Only when a request timeouts
  // which is at most once every 10 seconds per request (except tests).
  for (extensions::WindowController* window :
       *extensions::WindowControllerList::GetInstance()) {
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

}  // namespace ash::file_system_provider
