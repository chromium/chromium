// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_MODEL_CONFIG_LOADER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_MODEL_CONFIG_LOADER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/power/auto_screen_brightness/model_config_loader.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// This is a fake ModelConfigLoader used for testing only. To use it, we need to
// call its |set_model_config| first to set a specific model config, and then we
// can call |ReportModelConfigLoaded| so that it will notify its observers the
// set model config.
class FakeModelConfigLoader : public ModelConfigLoader {
 public:
  FakeModelConfigLoader();

  FakeModelConfigLoader(const FakeModelConfigLoader&) = delete;
  FakeModelConfigLoader& operator=(const FakeModelConfigLoader&) = delete;

  ~FakeModelConfigLoader() override;

  void set_model_config(const ModelConfig& model_config) {
    model_config_ = model_config;
    is_model_config_valid_ = IsValidModelConfig(model_config_);
    is_initialized_ = true;
  }

  // Notifies its observers the pre-specified model config.
  void ReportModelConfigLoaded();

  // ModelConfigLoader overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  // Notifies |observer| the specified model config if it's valid. Otherwise the
  // |observer| will receive a nullopt.
  void NotifyObserver(Observer* observer);

  bool is_initialized_ = false;
  bool is_model_config_valid_ = false;
  ModelConfig model_config_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<FakeModelConfigLoader> weak_ptr_factory_{this};
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_FAKE_MODEL_CONFIG_LOADER_H_
