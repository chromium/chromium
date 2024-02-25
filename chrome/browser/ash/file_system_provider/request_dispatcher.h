// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_DISPATCHER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_DISPATCHER_H_

#include <memory>
#include <optional>
#include <string>

namespace extensions {
struct Event;
}  // namespace extensions

namespace ash::file_system_provider {

// The interface for sending fileSystemProvider events created by |Operation|
// instances to an extension. The dispatcher is specific to a single extension.
class RequestDispatcher {
 public:
  virtual ~RequestDispatcher() = default;
  // Dispatch a fileSystemProvider request to the target extension.
  // |file_system_id| will be non-null for operations scoped to a specific
  // filesystem, and null for operations that don't apply to any existing
  // filesystem (like mount).
  virtual bool DispatchRequest(int request_id,
                               std::optional<std::string> file_system_id,
                               std::unique_ptr<extensions::Event> event) = 0;
  virtual void CancelRequest(int request_id,
                             std::optional<std::string> file_system_id) = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REQUEST_DISPATCHER_H_
