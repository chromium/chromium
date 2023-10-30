// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/storage/missive_storage_module.h"
#include "components/reporting/storage/missive_storage_module_delegate_impl.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

using ::chromeos::MissiveClient;

namespace reporting {

// static
bool StorageSelector::is_uploader_required() {
  // Ash needs to upload. LaCros cannot upload and does not need to.
  return BUILDFLAG(IS_CHROMEOS_ASH);
}

// static
bool StorageSelector::is_use_missive() {
  return true;  // Use missived storage.
}

// static
void StorageSelector::CreateMissiveStorageModule(
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
        cb) {
  MissiveClient* const missive_client = MissiveClient::Get();
  if (!missive_client) {
    std::move(cb).Run(base::unexpected(Status(
        error::FAILED_PRECONDITION,
        "Missive Client unavailable, probably has not been initialized")));
    return;
  }
  // Refer to the storage module.
  auto missive_storage_module_delegate =
      std::make_unique<MissiveStorageModuleDelegateImpl>(
          base::BindPostTask(missive_client->origin_task_runner(),
                             base::BindRepeating(&MissiveClient::EnqueueRecord,
                                                 missive_client->GetWeakPtr())),
          base::BindPostTask(
              missive_client->origin_task_runner(),
              base::BindRepeating(&MissiveClient::Flush,
                                  missive_client->GetWeakPtr())));
  auto missive_storage_module =
      MissiveStorageModule::Create(std::move(missive_storage_module_delegate));
  if (!missive_storage_module) {
    std::move(cb).Run(
        base::unexpected(Status(error::FAILED_PRECONDITION,
                                "Missive Storage Module failed to create")));
    return;
  }
  LOG(WARNING) << "Store reporting data by a Missive daemon";
  std::move(cb).Run(missive_storage_module);
  return;
}
}  // namespace reporting
