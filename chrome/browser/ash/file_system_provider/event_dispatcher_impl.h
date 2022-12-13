// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_EVENT_DISPATCHER_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_EVENT_DISPATCHER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/event_dispatcher.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace ash::file_system_provider {

class RequestManager;

// Routes fileSystemProvider events to an extension locally or in Lacros.
class EventDispatcherImpl : public EventDispatcher {
 public:
  EventDispatcherImpl(const extensions::ExtensionId& extension_id,
                      extensions::EventRouter* event_router,
                      RequestManager* request_manager);
  ~EventDispatcherImpl() override;

  EventDispatcherImpl(const EventDispatcherImpl&) = delete;
  EventDispatcherImpl& operator=(const EventDispatcherImpl&) = delete;

  bool DispatchEvent(int request_id,
                     absl::optional<std::string> file_system_id,
                     std::unique_ptr<extensions::Event> event) override;

 private:
  // This method is only used when Lacros is enabled. It's a callback from
  // Lacros indicating whether the operation was successfully forwarded. If the
  // operation could not be forwarded then the file system request manager must
  // be informed.
  void OperationForwarded(int request_id, bool delivery_failure);

  const extensions::ExtensionId extension_id_;
  const raw_ptr<extensions::EventRouter> event_router_;
  const raw_ptr<RequestManager> request_manager_;

  base::WeakPtrFactory<EventDispatcherImpl> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_EVENT_DISPATCHER_IMPL_H_
