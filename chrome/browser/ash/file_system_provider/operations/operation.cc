// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/operation.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_dispatcher.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/event_args.h"

namespace ash::file_system_provider::operations {

namespace {


}  // namespace

Operation::Operation(RequestDispatcher* dispatcher,
                     const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info), request_dispatcher_(dispatcher) {}

Operation::~Operation() = default;

bool Operation::SendEvent(
    int request_id,
    extensions::events::HistogramValue histogram_value,
    const std::string& event_name,
    scoped_refptr<const extensions::EventArgs> event_args) {
  auto event = std::make_unique<extensions::Event>(histogram_value, event_name,
                                                   std::move(event_args));
  return request_dispatcher_->DispatchRequest(
      request_id, file_system_info_.file_system_id(), std::move(event));
}

void Operation::OnAbort(int request_id) {
  request_dispatcher_->CancelRequest(request_id,
                                     file_system_info_.file_system_id());
}

}  // namespace ash::file_system_provider::operations
