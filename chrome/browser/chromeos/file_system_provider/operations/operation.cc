// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/operation.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Default implementation for dispatching an event. Can be replaced for unit
// tests by Operation::SetDispatchEventImplForTest().
bool DispatchEventImpl(extensions::EventRouter* event_router,
                       const extensions::ExtensionId& extension_id,
                       std::unique_ptr<extensions::Event> event) {
  if (!event_router->ExtensionHasEventListener(extension_id, event->event_name))
    return false;

  event_router->DispatchEventToExtension(extension_id, std::move(event));
  return true;
}

}  // namespace

Operation::Operation(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info),
      dispatch_event_impl_(base::BindRepeating(
          &DispatchEventImpl,
          event_router,
          file_system_info_.provider_id().GetExtensionId())) {}

Operation::~Operation() {
}

void Operation::SetDispatchEventImplForTesting(
    const DispatchEventImplCallback& callback) {
  dispatch_event_impl_ = callback;
}

bool Operation::SendEvent(int request_id,
                          extensions::events::HistogramValue histogram_value,
                          const std::string& event_name,
                          std::unique_ptr<base::ListValue> event_args) {
  return dispatch_event_impl_.Run(std::make_unique<extensions::Event>(
      histogram_value, event_name, std::move(event_args)));
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
