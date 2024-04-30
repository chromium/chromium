// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FAMILY_LINK_NOTICE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FAMILY_LINK_NOTICE_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class FamilyLinkNoticeView;
class ScopedSessionRefresher;

// Controller for the family link notice screen.
class FamilyLinkNoticeScreen : public BaseScreen {
 public:
  enum class Result { kDone, kSkipped };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit FamilyLinkNoticeScreen(base::WeakPtr<FamilyLinkNoticeView> view,
                                  const ScreenExitCallback& exit_callback);

  ~FamilyLinkNoticeScreen() override;

  FamilyLinkNoticeScreen(const FamilyLinkNoticeScreen&) = delete;
  FamilyLinkNoticeScreen& operator=(const FamilyLinkNoticeScreen&) = delete;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<FamilyLinkNoticeView> view_;

  // Keeps cryptohome authsession alive.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FAMILY_LINK_NOTICE_SCREEN_H_
