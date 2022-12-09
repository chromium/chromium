// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_MOUNT_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_MOUNT_REQUEST_HANDLER_H_

#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace ash::file_system_provider {

// Base class for operation bridges between fileapi and providing extensions.
class MountRequestHandler : public RequestManager::HandlerInterface {
 public:
  MountRequestHandler(extensions::EventRouter* event_router,
                      ProviderId provider_id,
                      RequestMountCallback callback);

  MountRequestHandler(const MountRequestHandler&) = delete;
  MountRequestHandler& operator=(const MountRequestHandler&) = delete;

  ~MountRequestHandler() override;

  // RequestManager::HandlerInterface overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 std::unique_ptr<RequestValue> result,
                 bool has_more) override;
  void OnError(int request_id,
               std::unique_ptr<RequestValue> result,
               base::File::Error error) override;

 private:
  using DispatchEventInternalCallback =
      base::RepeatingCallback<bool(int request_id)>;
  DispatchEventInternalCallback dispatch_event_impl_;
  RequestMountCallback callback_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_MOUNT_REQUEST_HANDLER_H_
