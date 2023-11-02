// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/menu_manager_test_observer.h"

#include "content/public/browser/browser_thread.h"

namespace extensions {

MenuManagerTestObserver::MenuManagerTestObserver(MenuManager* menu_manager)
    : menu_manager_(menu_manager) {
  observation_.Observe(menu_manager_.get());
}

MenuManagerTestObserver::~MenuManagerTestObserver() = default;

void MenuManagerTestObserver::WaitForExtension(
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The extension's menus may have already been loaded before we were
  // able to observe it.
  if (MenusItemsFound(extension_id))
    return;

  if (ids_with_reads_.count(extension_id) == 0) {
    waiting_for_id_ = extension_id;
    run_loop_.Run();
    DCHECK(MenusItemsFound(extension_id));
  }
}

void MenuManagerTestObserver::DidReadFromStorage(
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ids_with_reads_.insert(extension_id);
  if (extension_id == waiting_for_id_) {
    run_loop_.Quit();
  }
}

void MenuManagerTestObserver::WillWriteToStorage(
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ids_with_writes_.insert(extension_id);
  if (extension_id == waiting_for_id_) {
    run_loop_.Quit();
  }
}

bool MenuManagerTestObserver::MenusItemsFound(const ExtensionId& extension_id) {
  const MenuItem::ExtensionKey key(extension_id);
  return menu_manager_->MenuItems(key) &&
         !menu_manager_->MenuItems(key)->empty();
}

}  // namespace extensions
