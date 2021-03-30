// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FAMILY_LINK_NOTICE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FAMILY_LINK_NOTICE_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class FamilyLinkNoticeView;

// Controller for the family link notice screen.
class FamilyLinkNoticeScreen : public BaseScreen {
 public:
  enum class Result { DONE, SKIPPED };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit FamilyLinkNoticeScreen(FamilyLinkNoticeView* view,
                                  const ScreenExitCallback& exit_callback);

  ~FamilyLinkNoticeScreen() override;

  FamilyLinkNoticeScreen(const FamilyLinkNoticeScreen&) = delete;
  FamilyLinkNoticeScreen& operator=(const FamilyLinkNoticeScreen&) = delete;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(FamilyLinkNoticeView* view);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  FamilyLinkNoticeView* view_ = nullptr;

  ScreenExitCallback exit_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FAMILY_LINK_NOTICE_SCREEN_H_
