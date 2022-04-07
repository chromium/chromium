// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_HARDWARE_DATA_COLLECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_HARDWARE_DATA_COLLECTION_SCREEN_H_

#include <string>

#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/hardware_data_collection_screen_handler.h"

namespace ash {

// Representation independent class that controls OOBE screen showing HW data
// collection notice to users.
class HWDataCollectionScreen : public BaseScreen {
 public:
  enum class Result {
    ACCEPTED_WITH_HW_DATA_USAGE,
    ACCEPTED_WITHOUT_HW_DATA_USAGE,
    NOT_APPLICABLE,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  HWDataCollectionScreen(HWDataCollectionView* view,
                         const ScreenExitCallback& exit_callback);

  HWDataCollectionScreen(const HWDataCollectionScreen&) = delete;
  HWDataCollectionScreen& operator=(const HWDataCollectionScreen&) = delete;

  ~HWDataCollectionScreen() override;

  void SetHWDataUsageEnabled(bool enabled);

  // Returns true if user enabled HW data storage and usage.
  bool IsHWDataUsageEnabled() const;

  // This method is called, when view is being destroyed.
  void OnViewDestroyed(HWDataCollectionView* view);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;

  // HDDataCollectionView:
  void ShowHWDataUsageLearnMore();

  HWDataCollectionView* view_;

  bool hw_data_usage_enabled_ = true;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::HWDataCollectionScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_HARDWARE_DATA_COLLECTION_SCREEN_H_
