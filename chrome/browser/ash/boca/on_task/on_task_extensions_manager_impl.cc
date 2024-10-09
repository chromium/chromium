// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_extensions_manager_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/on_task/on_task_prefs.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

using extensions::Extension;
using extensions::ExtensionId;
using extensions::ExtensionIdList;
using extensions::ExtensionRegistry;
using extensions::ExtensionService;
using extensions::ExtensionSystem;
using extensions::ManagementPolicy;

namespace ash::boca {

OnTaskExtensionsManagerImpl::OnTaskExtensionsManagerImpl(Profile* profile)
    : profile_(profile) {
  // Re-enable extensions on init. This is needed should a device crash or
  // reboot so we can restore extensions to their previous enabled state.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OnTaskExtensionsManagerImpl::ReEnableExtensions,
                     weak_ptr_factory_.GetWeakPtr()));
}

OnTaskExtensionsManagerImpl::~OnTaskExtensionsManagerImpl() = default;

void OnTaskExtensionsManagerImpl::DisableExtensions() {
  ExtensionService* const extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  const ExtensionRegistry* const extension_registry =
      ExtensionRegistry::Get(profile_);
  ExtensionIdList disabled_extension_ids;
  for (const auto& extension_id :
       extension_registry->enabled_extensions().GetIDs()) {
    const Extension* const extension =
        extension_registry->enabled_extensions().GetByID(extension_id);
    if (CanDisableExtension(extension)) {
      // We use `DISABLE_USER_ACTION` disable reason despite its support with
      // extension sync for now. This remains consistent with extension
      // management through the Assessment Assistant extension used with locked
      // quizzes. This may be adjusted for both components accordingly.
      extension_service->DisableExtension(
          extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
      disabled_extension_ids.push_back(extension_id);
    }
  }

  SaveDisabledExtensionIds(disabled_extension_ids);
}

void OnTaskExtensionsManagerImpl::ReEnableExtensions() {
  ExtensionService* const extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  const ExtensionRegistry* const extension_registry =
      ExtensionRegistry::Get(profile_);
  const base::Value::List& disabled_extension_ids =
      profile_->GetPrefs()->GetList(kDisabledOnTaskExtensions);
  for (const auto& disabled_extension_id : disabled_extension_ids) {
    const ExtensionId& extension_id =
        static_cast<ExtensionId>(disabled_extension_id.GetString());
    const Extension* const extension =
        extension_registry->disabled_extensions().GetByID(extension_id);
    if (extension && CanEnableExtension(extension)) {
      extension_service->EnableExtension(extension_id);
    }
  }

  // Clear tracked extension ids now that they have been processed.
  SaveDisabledExtensionIds({});
}

bool OnTaskExtensionsManagerImpl::CanDisableExtension(
    const Extension* extension) {
  CHECK(extension);
  bool is_component_extension =
      extensions::Manifest::IsComponentLocation(extension->location());
  const ManagementPolicy* const policy =
      ExtensionSystem::Get(profile_)->management_policy();
  return !is_component_extension &&
         !policy->MustRemainEnabled(extension, /*error=*/nullptr);
}

bool OnTaskExtensionsManagerImpl::CanEnableExtension(
    const Extension* extension) {
  CHECK(extension);
  const ManagementPolicy* const policy =
      ExtensionSystem::Get(profile_)->management_policy();
  return !policy->MustRemainDisabled(extension, /*reason=*/nullptr,
                                     /*error=*/nullptr);
}

void OnTaskExtensionsManagerImpl::SaveDisabledExtensionIds(
    const ExtensionIdList& extension_ids) {
  ScopedListPrefUpdate pref_update(profile_->GetPrefs(),
                                   kDisabledOnTaskExtensions);
  base::Value::List& saved_extension_ids = pref_update.Get();
  saved_extension_ids.clear();
  for (const auto& extension_id : extension_ids) {
    saved_extension_ids.Append(extension_id);
  }
}

}  // namespace ash::boca
