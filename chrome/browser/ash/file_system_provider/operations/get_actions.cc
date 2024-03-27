// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/get_actions.h"

#include <algorithm>
#include <string>
#include <utility>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash::file_system_provider::operations {
namespace {

// Convert the request |value| into a list of actions.
Actions ConvertRequestValueToActions(const RequestValue& value) {
  using extensions::api::file_system_provider_internal::
      GetActionsRequestedSuccess::Params;

  const Params* params = value.get_actions_success_params();
  DCHECK(params);

  Actions result;
  for (const auto& idl_action : params->actions) {
    Action action;
    action.id = idl_action.id;
    action.title = idl_action.title.value_or(std::string());
    result.push_back(action);
  }

  return result;
}

}  // namespace

GetActions::GetActions(RequestDispatcher* dispatcher,
                       const ProvidedFileSystemInfo& file_system_info,
                       const std::vector<base::FilePath>& entry_paths,
                       ProvidedFileSystemInterface::GetActionsCallback callback)
    : Operation(dispatcher, file_system_info),
      entry_paths_(entry_paths),
      callback_(std::move(callback)) {}

GetActions::~GetActions() = default;

bool GetActions::Execute(int request_id) {
  using extensions::api::file_system_provider::GetActionsRequestedOptions;

  GetActionsRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  for (const auto& entry_path : entry_paths_)
    options.entry_paths.push_back(entry_path.AsUTF8Unsafe());

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_GET_ACTIONS_REQUESTED,
      extensions::api::file_system_provider::OnGetActionsRequested::kEventName,
      extensions::api::file_system_provider::OnGetActionsRequested::Create(
          options));
}

void GetActions::OnSuccess(/*request_id=*/int,
                           const RequestValue& result,
                           bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(ConvertRequestValueToActions(result),
                           base::File::FILE_OK);
}

void GetActions::OnError(/*request_id=*/int,
                         /*result=*/const RequestValue&,
                         base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(Actions(), error);
}

}  // namespace ash::file_system_provider::operations
