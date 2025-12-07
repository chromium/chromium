// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace glic {

class GlicButtonControllerDelegate;
class GlicKeyedService;

// Controller class for the button entry point. Manages visibility, icon
// and attachment indicator for the button.
class GlicButtonController {
 public:
  GlicButtonController(Profile* profile,
                       BrowserWindowInterface& browser,
                       GlicButtonControllerDelegate* delegate,
                       GlicKeyedService* service);
  ~GlicButtonController();

 private:
  void UpdateButton();

  raw_ptr<Profile> profile_;
  raw_ref<BrowserWindowInterface> browser_;
  raw_ptr<GlicButtonControllerDelegate> glic_controller_delegate_;
  raw_ptr<GlicKeyedService> glic_keyed_service_;
  PrefChangeRegistrar pref_registrar_;

  // Holds subscriptions for callbacks.
  std::vector<base::CallbackListSubscription> subscriptions_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_H_
