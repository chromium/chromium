// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_PREFS_H_
#define ASH_ACCELERATORS_ACCELERATOR_PREFS_H_

#include "ash/accelerators/accelerator_prefs_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
// `AcceleratorPrefs` manages shortcut preference settings. It is used to
// register the prefs and observe the change in shortcut policy. It sends an
// updated to the |accelerator_controller_impl| and
// |accelerator_configuration_provider| whenever one of the shortcut policy
// changes.
class ASH_EXPORT AcceleratorPrefs : public SessionObserver {
 public:
  // This Observer class is used to observe changes to the shortcut policy.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    // Called when shortcut policy changes.
    virtual void OnShortcutPolicyUpdated() = 0;
  };

  explicit AcceleratorPrefs(std::unique_ptr<AcceleratorPrefsDelegate> delegate);
  AcceleratorPrefs(const AcceleratorPrefs&) = delete;
  AcceleratorPrefs& operator=(const AcceleratorPrefs&) = delete;
  ~AcceleratorPrefs() override;

  // static
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Add and remove observers for shortcut policy changes.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ObservePrefs(PrefService* prefs);
  void OnCustomizationPolicyChanged();
  bool IsCustomizationAllowed();
  bool IsCustomizationAllowedByPolicy();
  bool IsUserEnterpriseManaged();

 private:
  // The delegate responsible for communicating with between Ash and the
  // Browser.
  std::unique_ptr<AcceleratorPrefsDelegate> delegate_;
  // List of observers for shortcut policy changes.
  base::ObserverList<Observer> observers_;
  // Registrar for observing pref changes.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_PREFS_H_
