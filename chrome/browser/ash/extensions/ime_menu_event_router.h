// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_IME_MENU_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_IME_MENU_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Event router class for the IME Menu activated/deactivate events.
// It's owned by InputMethodManager, and its lifetime restricted to the lifetime
// of the InputMethodManager and the EventRouter.
class ExtensionImeMenuEventRouter
    : public ash::input_method::InputMethodManager::ImeMenuObserver {
 public:
  explicit ExtensionImeMenuEventRouter(content::BrowserContext* context);

  ExtensionImeMenuEventRouter(const ExtensionImeMenuEventRouter&) = delete;
  ExtensionImeMenuEventRouter& operator=(const ExtensionImeMenuEventRouter&) =
      delete;

  ~ExtensionImeMenuEventRouter() override;

  // ash::input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool activation) override;
  void ImeMenuListChanged() override;
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<ash::input_method::InputMethodManager::MenuItem>& items)
      override;

 private:
  raw_ptr<content::BrowserContext> context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_IME_MENU_EVENT_ROUTER_H_
