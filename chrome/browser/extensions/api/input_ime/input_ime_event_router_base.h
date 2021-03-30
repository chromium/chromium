// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_EVENT_ROUTER_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_EVENT_ROUTER_BASE_H_

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/ime/chromeos/ime_engine_handler_interface.h"

namespace extensions {

class InputImeEventRouterBase {
 public:
  explicit InputImeEventRouterBase(Profile* profile);
  virtual ~InputImeEventRouterBase();

  // Gets the input method engine if the extension is active.
  virtual chromeos::InputMethodEngine* GetEngineIfActive(
      const std::string& extension_id,
      std::string* error) = 0;

  Profile* GetProfile() const { return profile_; }

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouterBase);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_EVENT_ROUTER_BASE_H_
