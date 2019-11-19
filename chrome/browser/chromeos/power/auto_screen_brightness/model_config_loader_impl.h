// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODEL_CONFIG_LOADER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODEL_CONFIG_LOADER_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/model_config_loader.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

// Real implementation of ModelConfigLoader. It reads model params from the disk
// first and then overrides certain fields from experiment flags (if available).
class ModelConfigLoaderImpl : public ModelConfigLoader {
 public:
  ModelConfigLoaderImpl();
  ~ModelConfigLoaderImpl() override;

  // ModelConfigLoader overrides:
  void AddObserver(ModelConfigLoader::Observer* observer) override;
  void RemoveObserver(ModelConfigLoader::Observer* observer) override;

  // Only used for testing so that we can supply a test/fake |model_params_path|
  // and |blocking_task_runner|.
  static std::unique_ptr<ModelConfigLoaderImpl> CreateForTesting(
      const base::FilePath& model_params_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

 private:
  ModelConfigLoaderImpl(
      const base::FilePath& model_params_path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      bool is_testing);

  // Reads params from the disk and then calls InitFromParams.
  void Init();

  // Overrides loaded model configs from experiment flags. It will call
  // OnInitializationComplete after completion.
  void InitFromParams();

  void OnModelParamsLoadedFromDisk(const std::string& content);

  // Notifies observers whether a valid model config exists. Called after we've
  // attempted to read from the disk and override with experiment flags.
  void OnInitializationComplete();

  base::FilePath model_params_path_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  bool is_testing_ = false;

  base::ObserverList<ModelConfigLoaderImpl::Observer> observers_;

  ModelConfig model_config_;
  bool is_model_config_valid_ = false;
  bool is_initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModelConfigLoaderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModelConfigLoaderImpl);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_MODEL_CONFIG_LOADER_IMPL_H_
