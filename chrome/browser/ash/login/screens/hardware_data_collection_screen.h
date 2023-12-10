// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_HARDWARE_DATA_COLLECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_HARDWARE_DATA_COLLECTION_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class HWDataCollectionView;
class ScopedSessionRefresher;

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
  HWDataCollectionScreen(base::WeakPtr<HWDataCollectionView> view,
                         const ScreenExitCallback& exit_callback);

  HWDataCollectionScreen(const HWDataCollectionScreen&) = delete;
  HWDataCollectionScreen& operator=(const HWDataCollectionScreen&) = delete;

  ~HWDataCollectionScreen() override;

  void SetHWDataUsageEnabled(bool enabled);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Keeps cryptohome authsession alive.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  base::WeakPtr<HWDataCollectionView> view_;

  bool hw_data_usage_enabled_ = true;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_HARDWARE_DATA_COLLECTION_SCREEN_H_
