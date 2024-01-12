// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Event router class for the input method events.
class ExtensionInputMethodEventRouter
    : public ash::input_method::InputMethodManager::Observer {
 public:
  explicit ExtensionInputMethodEventRouter(content::BrowserContext* context);

  ExtensionInputMethodEventRouter(const ExtensionInputMethodEventRouter&) =
      delete;
  ExtensionInputMethodEventRouter& operator=(
      const ExtensionInputMethodEventRouter&) = delete;

  ~ExtensionInputMethodEventRouter() override;

  // Implements ash::input_method::InputMethodManager::Observer:
  void InputMethodChanged(ash::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

 private:
  raw_ptr<content::BrowserContext> context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_
