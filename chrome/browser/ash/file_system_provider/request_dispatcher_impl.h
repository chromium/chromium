// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_DISPATCHER_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_DISPATCHER_IMPL_H_

#include <memory>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_system_provider/request_dispatcher.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace ash::file_system_provider {

class ServiceWorkerLifetimeManager;

// Routes fileSystemProvider events to an extension.
class RequestDispatcherImpl : public RequestDispatcher {
 public:
  // This callback is an indirection to reject requests in |RequestManager|: the
  // dispatcher cannot depend on request manager directly as it needs to outlive
  // the request manager the request manager accesses the dispatcher in the
  // destructor when it aborts outstanding operations.
  using ForwardResultCallback =
      base::RepeatingCallback<void(int, base::File::Error)>;
  RequestDispatcherImpl(const extensions::ExtensionId& extension_id,
                        extensions::EventRouter* event_router,
                        ServiceWorkerLifetimeManager* sw_lifetime_manager);
  ~RequestDispatcherImpl() override;

  RequestDispatcherImpl(const RequestDispatcherImpl&) = delete;
  RequestDispatcherImpl& operator=(const RequestDispatcherImpl&) = delete;

  bool DispatchRequest(int request_id,
                       std::optional<std::string> file_system_id,
                       std::unique_ptr<extensions::Event> event) override;
  void CancelRequest(int request_id,
                     std::optional<std::string> file_system_id) override;

 private:
  const extensions::ExtensionId extension_id_;
  const raw_ptr<extensions::EventRouter> event_router_;
  const raw_ptr<ServiceWorkerLifetimeManager> sw_lifetime_manager_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_DISPATCHER_IMPL_H_
