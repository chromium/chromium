// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_MULTITASK_MENU_NUDGE_DELEGATE_LACROS_H_
#define CHROME_BROWSER_LACROS_MULTITASK_MENU_NUDGE_DELEGATE_LACROS_H_

#include "base/values.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"

// Lacros implementation of the nudge controller delegate that lets us get and
// set pref values from the ash active profile via mojo.
class MultitaskMenuNudgeDelegateLacros
    : public chromeos::MultitaskMenuNudgeController::Delegate {
 public:
  using GetPreferencesCallback =
      chromeos::MultitaskMenuNudgeController::GetPreferencesCallback;

  static constexpr int kTabletNudgeAdditionalYOffset = 6;

  MultitaskMenuNudgeDelegateLacros();
  MultitaskMenuNudgeDelegateLacros(const MultitaskMenuNudgeDelegateLacros&) =
      delete;
  MultitaskMenuNudgeDelegateLacros& operator=(
      const MultitaskMenuNudgeDelegateLacros&) = delete;
  ~MultitaskMenuNudgeDelegateLacros() override;

  // chromeos::MultitaskMenuNudgeController::Delegate:
  int GetTabletNudgeYOffset() const override;
  void GetNudgePreferences(bool tablet_mode,
                           GetPreferencesCallback callback) override;
  void SetNudgePreferences(bool tablet_mode,
                           int count,
                           base::Time time) override;
  bool IsUserNewOrGuest() const override;

 private:
  using PrefPair = std::pair<crosapi::mojom::PrefPath, base::Value>;

  // Callback ran when we got either pref from the pref service. Runs
  // `callback`, which is part of a barrier callback.
  void OnGetPreference(base::OnceCallback<void(PrefPair)> callback,
                       crosapi::mojom::PrefPath pref_path,
                       std::optional<base::Value> value);

  // Callback ran when we got both our prefs from the pref service. Parses the
  // values and then uses `callback` to send them to the prefs requester.
  void OnGotAllPreferences(GetPreferencesCallback callback,
                           std::vector<PrefPair> pref_values);

  base::WeakPtrFactory<MultitaskMenuNudgeDelegateLacros> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_LACROS_MULTITASK_MENU_NUDGE_DELEGATE_LACROS_H_
