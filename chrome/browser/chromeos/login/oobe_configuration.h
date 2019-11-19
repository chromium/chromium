// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_CONFIGURATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_CONFIGURATION_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"

namespace chromeos {

// Configuration that might be used to automate passing through
// OOBE/enrollment screens
class OobeConfiguration {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    virtual void OnOobeConfigurationChanged() = 0;
  };

  OobeConfiguration();
  virtual ~OobeConfiguration();

  const base::Value& GetConfiguration() const;
  bool CheckCompleted() const;

  void AddAndFireObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ResetConfiguration();
  void CheckConfiguration();

  // Returns current OobeConfiguration instance and NULL if it hasn't been
  // initialized yet.
  static OobeConfiguration* Get();

  static void set_skip_check_for_testing(bool value) {
    skip_check_for_testing_ = value;
  }

 private:
  void OnConfigurationCheck(bool has_configuration,
                            const std::string& configuration);
  void NotifyObservers();

  void UpdateConfigurationValues();

  // Pointer to the existing OobeConfiguration instance (if any).
  // Set in ctor, reset in dtor. Not owned since we need to control the lifetime
  // externally.
  // Instance is owned by ChromeSessionManager
  static OobeConfiguration* instance;

  // Some tests might require setting some expectations before configuration is
  // loaded. This field allows tests to skip actual configuration check in
  // CheckConfiguration() call. Once all expectations are set, test should set
  // this flag back to false and run CheckConfiguration() to proceed.
  static bool skip_check_for_testing_;

  // Tracks if configuration check is completed.
  bool check_completed_;

  // Non-null dictionary value with configuration.
  std::unique_ptr<base::Value> configuration_;

  // Observers
  base::ObserverList<Observer>::Unchecked observer_list_;

  // Factory of callbacks.
  base::WeakPtrFactory<OobeConfiguration> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_CONFIGURATION_H_
