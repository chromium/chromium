// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_EXTENSIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_EXTENSIONS_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/on_task/on_task_extensions_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class Extension;
}

class Profile;

namespace ash::boca {

// `OnTaskExtensionsManager` implementation used for managing extensions in the
// context of OnTask.
class OnTaskExtensionsManagerImpl : public OnTaskExtensionsManager {
 public:
  explicit OnTaskExtensionsManagerImpl(Profile* profile);
  ~OnTaskExtensionsManagerImpl() override;

  // OnTaskExtensionsManager:
  void DisableExtensions() override;
  void ReEnableExtensions() override;

 private:
  // Returns whether the specified extension can be disabled for OnTask. Some
  // extensions like component extensions as well as those enabled by policy
  // should not be disabled.
  bool CanDisableExtension(const extensions::Extension* extension);

  // Returns whether the specified extension can be enabled for OnTask. Normally
  // used while re-enabling extensions when in an unlocked setting as a sanity
  // check (for example, policy changes).
  bool CanEnableExtension(const extensions::Extension* extension);

  // Persists specified disabled extension ids to the user pref store.
  void SaveDisabledExtensionIds(
      const extensions::ExtensionIdList& extension_ids);

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<OnTaskExtensionsManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_EXTENSIONS_MANAGER_IMPL_H_
