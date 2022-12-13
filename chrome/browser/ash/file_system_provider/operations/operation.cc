// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/operation.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/ash/file_system_provider/event_dispatcher.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "extensions/browser/event_router.h"

namespace ash {
namespace file_system_provider {
namespace operations {

namespace {


}  // namespace

Operation::Operation(EventDispatcher* dispatcher,
                     const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info), event_dispatcher_(dispatcher) {}

Operation::~Operation() {
}

bool Operation::SendEvent(int request_id,
                          extensions::events::HistogramValue histogram_value,
                          const std::string& event_name,
                          base::Value::List event_args) {
  auto event = std::make_unique<extensions::Event>(histogram_value, event_name,
                                                   std::move(event_args));
  return event_dispatcher_->DispatchEvent(
      request_id, file_system_info_.file_system_id(), std::move(event));
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash
