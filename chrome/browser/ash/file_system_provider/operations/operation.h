// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_OPERATION_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_OPERATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;
class RequestDispatcher;

namespace operations {

// Base class for operation bridges between fileapi and providing extensions.
class Operation : public RequestManager::HandlerInterface {
 public:
  Operation(RequestDispatcher* dispatcher,
            const ProvidedFileSystemInfo& file_system_info);

  Operation(const Operation&) = delete;
  Operation& operator=(const Operation&) = delete;

  ~Operation() override;

  // RequestManager::HandlerInterface overrides.
  bool Execute(int request_id) override = 0;
  void OnSuccess(int request_id,
                 const RequestValue& result,
                 bool has_more) override = 0;
  void OnError(int request_id,
               const RequestValue& result,
               base::File::Error error) override = 0;
  void OnAbort(int request_id) override;

 protected:
  // Sends an event to the providing extension. Returns false, if the providing
  // extension does not handle the |event_name| event.
  bool SendEvent(int request_id,
                 extensions::events::HistogramValue histogram_value,
                 const std::string& event_name,
                 base::Value::List event_args);

  ProvidedFileSystemInfo file_system_info_;

 private:
  raw_ptr<RequestDispatcher, DanglingUntriaged> request_dispatcher_;
};

}  // namespace operations
}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_OPERATION_H_
