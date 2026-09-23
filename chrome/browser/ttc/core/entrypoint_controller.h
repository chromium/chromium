// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_ENTRYPOINT_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_ENTRYPOINT_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/states.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace ttc {

class TtcKeyedService;

enum class EntrypointType {
  kToolbarButton = 0,
  kAppMenu = 1,
  kMaxValue = kAppMenu,
};

// Window-scoped controller for TTC's entrypoints. Routes entrypoint activations
// to the profile-scoped TtcKeyedService, and reflects the change in the
// entrypoints UI. Since a session is profile-scoped, every window's
// controller reacts to the same state change, keeping all windows consistent.
class EntrypointController {
 public:
  DECLARE_USER_DATA(EntrypointController);
  static EntrypointController* From(BrowserWindowInterface* browser);

  explicit EntrypointController(BrowserWindowInterface& browser,
                                TtcKeyedService& service);
  ~EntrypointController();
  EntrypointController(const EntrypointController&) = delete;
  EntrypointController& operator=(const EntrypointController&) = delete;

  void EntrypointHandler(EntrypointType type);

 private:
  void ToolbarButtonHandler();
  void AppMenuHandler();
  void OnTtcStateChanged(ServiceState state);
  void UpdateUi(ServiceState state);
  void UpdateToolbarButton(ServiceState state);

  void ToggleSession();

  const raw_ref<BrowserWindowInterface> browser_;
  // Safe because the service is owned by the profile, which outlives the
  // browser window.
  const raw_ref<TtcKeyedService> service_;

  ui::ScopedUnownedUserData<EntrypointController> scoped_unowned_user_data_;

  base::CallbackListSubscription ttc_state_subscription_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_ENTRYPOINT_CONTROLLER_H_
