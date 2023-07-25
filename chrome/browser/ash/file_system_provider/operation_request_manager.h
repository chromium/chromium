// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATION_REQUEST_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATION_REQUEST_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"

class Profile;

namespace ash::file_system_provider {

// Manages requests for provided file system operations by assigning them a
// request id and timing out slow requests. Note: Those requests are all
// requests that have a fileSystemId, which is all requests except
// onMountRequested, handled by another instance of RequestManager.
class OperationRequestManager : public RequestManager {
 public:
  // Creates a request manager for |profile| and |provider_id|. Note, that
  // there is one instance per provided file system, thus potentially be
  // multiple instances per provider.
  OperationRequestManager(Profile* profile,
                          const std::string& provider_id,
                          NotificationManagerInterface* notification_manager,
                          base::TimeDelta timeout);

  OperationRequestManager(const OperationRequestManager&) = delete;
  OperationRequestManager& operator=(const OperationRequestManager&) = delete;
  ~OperationRequestManager() override;

  // Indicate start/end of an operation that involves user interaction. As long
  // as least one interaction is active, "unresponsive operation" notifications
  // won't be shown. Not intended to be called directly, but via
  // `ProvidedFileSystem::StartUserInteraction` instead.
  void StartUserInteraction() { current_user_interactions_++; }
  void EndUserInteraction() { current_user_interactions_--; }

 private:
  // Called when a request with |request_id| times out.
  void OnRequestTimeout(int request_id) override;

  // Checks whether there is an ongoing interaction between the provider
  // and user.
  bool IsInteractingWithUser() const;

  std::string provider_id_;
  int current_user_interactions_ = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATION_REQUEST_MANAGER_H_
