// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_REGISTRY_H_

#include <memory>
#include <vector>

#include "base/macros.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace send_tab_to_self {

class ReceivingUiHandler;

// Registry responsible for keeping track of which UI handlers are appropriate
// for each platform. A platform can have multiple handlers which are
// called in the order specified.
// Singleton.
class ReceivingUiHandlerRegistry {
 public:
  // Returns the singleton instance of this class.
  static ReceivingUiHandlerRegistry* GetInstance();
  void InstantiatePlatformSpecificHandlers(Profile* profile_);

  // Returns all the handlers to perform UI updates for the platform.
  // Called by the SendTabToSelfClientService.
  const std::vector<std::unique_ptr<ReceivingUiHandler>>& GetHandlers() const;

 private:
  friend struct base::DefaultSingletonTraits<ReceivingUiHandlerRegistry>;

  ReceivingUiHandlerRegistry();
  ~ReceivingUiHandlerRegistry();
  std::vector<std::unique_ptr<ReceivingUiHandler>> applicable_handlers_;
  DISALLOW_COPY_AND_ASSIGN(ReceivingUiHandlerRegistry);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_REGISTRY_H_
