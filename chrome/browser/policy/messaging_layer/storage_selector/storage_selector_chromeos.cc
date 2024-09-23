// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage_selector/storage_selector.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/dbus/missive/missive_storage_module.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

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
  ::chromeos::MissiveStorageModule::Create(std::move(cb));
}
}  // namespace reporting
