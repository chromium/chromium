// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_MOUNT_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_MOUNT_REQUEST_HANDLER_H_

#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"

namespace ash::file_system_provider {

class RequestDispatcher;

// Base class for operation bridges between fileapi and providing extensions.
class MountRequestHandler : public RequestManager::HandlerInterface {
 public:
  MountRequestHandler(RequestDispatcher* dispatcher,
                      RequestMountCallback callback);

  MountRequestHandler(const MountRequestHandler&) = delete;
  MountRequestHandler& operator=(const MountRequestHandler&) = delete;

  ~MountRequestHandler() override;

  // RequestManager::HandlerInterface overrides.
  bool Execute(int request_id) override;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override;
  void OnAbort(int request_id) override;

 private:
  raw_ptr<RequestDispatcher, DanglingUntriaged> request_dispatcher_;
  RequestMountCallback callback_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_MOUNT_REQUEST_HANDLER_H_
