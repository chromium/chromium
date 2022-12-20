// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/configure.h"

#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider.h"

namespace ash {
namespace file_system_provider {
namespace operations {

Configure::Configure(RequestDispatcher* dispatcher,
                     const ProvidedFileSystemInfo& file_system_info,
                     storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info), callback_(std::move(callback)) {}

Configure::~Configure() {
}

bool Configure::Execute(int request_id) {
  using extensions::api::file_system_provider::ConfigureRequestedOptions;

  ConfigureRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_CONFIGURE_REQUESTED,
      extensions::api::file_system_provider::OnConfigureRequested::kEventName,
      extensions::api::file_system_provider::OnConfigureRequested::Create(
          options));
}

void Configure::OnSuccess(int /* request_id */,
                          std::unique_ptr<RequestValue> /* result */,
                          bool /* has_more */) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void Configure::OnError(int /* request_id */,
                        std::unique_ptr<RequestValue> /* result */,
                        base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash
