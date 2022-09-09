// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_REGISTRY_H_

#include <memory>
#include <vector>

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace send_tab_to_self {

class AndroidNotificationHandler;
class ReceivingUiHandler;
class SendTabToSelfToolbarIconController;

// Registry responsible for keeping track of which UI handlers are appropriate
// for each platform. A platform can have multiple handlers which are
// called in the order specified.
// Singleton.
class ReceivingUiHandlerRegistry {
 public:
  // Returns the singleton instance of this class.
  static ReceivingUiHandlerRegistry* GetInstance();

  ReceivingUiHandlerRegistry(const ReceivingUiHandlerRegistry&) = delete;
  ReceivingUiHandlerRegistry& operator=(const ReceivingUiHandlerRegistry&) =
      delete;

  void InstantiatePlatformSpecificHandlers(Profile* profile_);

  // Returns all the handlers to perform UI updates for the platform.
  // Called by the SendTabToSelfClientService.
  const std::vector<std::unique_ptr<ReceivingUiHandler>>& GetHandlers() const;

  // Return the SendTabToSelfToolbarIconController owned by the registry
  // for the given |profile|.
  SendTabToSelfToolbarIconController* GetToolbarButtonControllerForProfile(
      Profile* profile);

  AndroidNotificationHandler* GetAndroidNotificationHandlerForProfile(
      Profile* profile);

  void OnProfileShutdown(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<ReceivingUiHandlerRegistry>;

  ReceivingUiHandlerRegistry();
  ~ReceivingUiHandlerRegistry();
  std::vector<std::unique_ptr<ReceivingUiHandler>> applicable_handlers_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_REGISTRY_H_
