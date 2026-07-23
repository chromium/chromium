// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;

namespace glic {

class GlicSplitButtonDelegate;
class GlicKeyedService;

// Controller class for the button entry point. Manages visibility, icon
// and attachment indicator for the button.
class GlicButtonController {
 public:
  DECLARE_USER_DATA(GlicButtonController);

  static GlicButtonController* From(BrowserWindowInterface* browser);
  GlicButtonController(Profile* profile,
                       BrowserWindowInterface& browser,
                       GlicKeyedService* service);
  ~GlicButtonController();

  // TODO(crbug.com/511309088): Remove these and have the split button
  // controller keep the delegates.
  void SetHorizontalTabsDelegate(GlicSplitButtonDelegate* delegate);
  void SetVerticalTabsDelegate(GlicSplitButtonDelegate* delegate);

  base::WeakPtr<GlicButtonController> GetWeakPtr();

  mojom::InvocationSource GetInvocationSource(bool is_showing_nudge,
                                              bool is_toolbar) const;

 private:
  void UpdateButton();

  raw_ptr<Profile> profile_;
  raw_ref<BrowserWindowInterface> browser_;
  raw_ptr<GlicSplitButtonDelegate> tab_strip_glic_controller_delegate_ =
      nullptr;
  raw_ptr<GlicSplitButtonDelegate> toolbar_glic_controller_delegate_ = nullptr;
  raw_ptr<GlicKeyedService> glic_keyed_service_;
  PrefChangeRegistrar pref_registrar_;

  // Holds subscriptions for callbacks.
  std::vector<base::CallbackListSubscription> subscriptions_;

  ui::ScopedUnownedUserData<GlicButtonController> scoped_unowned_user_data_;

  base::WeakPtrFactory<GlicButtonController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_H_
