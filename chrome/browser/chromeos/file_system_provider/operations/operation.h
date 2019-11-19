// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_OPERATION_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace base {
class ListValue;
}

namespace extensions {
struct Event;
class EventRouter;
}

namespace chromeos {
namespace file_system_provider {
namespace operations {

// Base class for operation bridges between fileapi and providing extensions.
class Operation : public RequestManager::HandlerInterface {
 public:
  typedef base::Callback<bool(std::unique_ptr<extensions::Event> event)>
      DispatchEventImplCallback;

  Operation(extensions::EventRouter* event_router,
            const ProvidedFileSystemInfo& file_system_info);
  ~Operation() override;

  // RequestManager::HandlerInterface overrides.
  bool Execute(int request_id) override = 0;
  void OnSuccess(int request_id,
                 std::unique_ptr<RequestValue> result,
                 bool has_more) override = 0;
  void OnError(int request_id,
               std::unique_ptr<RequestValue> result,
               base::File::Error error) override = 0;

  // Sets custom dispatchign event implementation for tests.
  void SetDispatchEventImplForTesting(
      const DispatchEventImplCallback& callback);

 protected:
  // Sends an event to the providing extension. Returns false, if the providing
  // extension does not handle the |event_name| event.
  bool SendEvent(int request_id,
                 extensions::events::HistogramValue histogram_value,
                 const std::string& event_name,
                 std::unique_ptr<base::ListValue> event_args);

  ProvidedFileSystemInfo file_system_info_;

 private:
  DispatchEventImplCallback dispatch_event_impl_;
  DISALLOW_COPY_AND_ASSIGN(Operation);
};

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_OPERATION_H_
