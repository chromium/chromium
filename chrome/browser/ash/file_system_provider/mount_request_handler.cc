// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/mount_request_handler.h"

#include <utility>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/request_dispatcher_impl.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/event_router.h"

namespace ash::file_system_provider {

MountRequestHandler::MountRequestHandler(RequestDispatcher* dispatcher,
                                         RequestMountCallback callback)
    : request_dispatcher_(dispatcher), callback_(std::move(callback)) {}

MountRequestHandler::~MountRequestHandler() = default;

bool MountRequestHandler::Execute(int request_id) {
  base::Value::List event_args;
  event_args.reserve(1);
  event_args.Append(base::Value(request_id));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::FILE_SYSTEM_PROVIDER_ON_MOUNT_REQUESTED,
      extensions::api::file_system_provider::OnMountRequested::kEventName,
      std::move(event_args));

  return request_dispatcher_->DispatchRequest(request_id, std::nullopt,
                                              std::move(event));
}

void MountRequestHandler::OnSuccess(/*request_id=*/int,
                                    /*result=*/const RequestValue&,
                                    bool has_more) {
  // File handle is the same as request id of the OpenFile operation.
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void MountRequestHandler::OnError(/*request_id=*/int,
                                  /*result=*/const RequestValue&,
                                  base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

void MountRequestHandler::OnAbort(int request_id) {
  request_dispatcher_->CancelRequest(request_id, std::nullopt);
}

}  // namespace ash::file_system_provider
