// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MENU_MANAGER_TEST_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_MENU_MANAGER_TEST_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/menu_manager.h"

#include <set>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class MenuManagerTestObserver : public MenuManager::TestObserver {
 public:
  explicit MenuManagerTestObserver(MenuManager* menu_manager);

  ~MenuManagerTestObserver() override;

  MenuManagerTestObserver(const MenuManagerTestObserver&) = delete;
  MenuManagerTestObserver& operator=(const MenuManagerTestObserver&) = delete;

  // MenuManager::TestObserver overrides.
  void DidReadFromStorage(const ExtensionId& extension_id) override;
  void WillWriteToStorage(const ExtensionId& extension_id) override;

  // Wait for a MenuManager storage read or write for the specified
  // extension.
  void WaitForExtension(const ExtensionId& extension_id);

  bool did_read_for_extension(const ExtensionId& extension_id) const {
    return ids_with_reads_.count(extension_id) != 0;
  }

  bool will_write_for_extension(const ExtensionId& extension_id) const {
    return ids_with_writes_.count(extension_id) != 0;
  }

 private:
  bool MenusItemsFound(const ExtensionId& extension_id);

  const raw_ptr<MenuManager> menu_manager_;
  std::set<ExtensionId> ids_with_reads_;
  std::set<ExtensionId> ids_with_writes_;
  ExtensionId waiting_for_id_;
  base::RunLoop run_loop_;
  base::ScopedObservation<MenuManager, MenuManager::TestObserver> observation_{
      this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MENU_MANAGER_TEST_OBSERVER_H_
