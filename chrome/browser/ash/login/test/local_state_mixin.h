// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_STATE_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_STATE_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace chromeos {

// Mixin browser tests can use for setting up local state environment.
class LocalStateMixin : public InProcessBrowserTestMixin {
 public:
  class Delegate {
   public:
    void SetUpLocalStateBase();

    ~Delegate();

   private:
    // Implement this function to setup g_browser_process->local_state()
    virtual void SetUpLocalState() = 0;

    bool setup_called_ = false;
  };
  LocalStateMixin(InProcessBrowserTestMixinHost* host, Delegate* delegate);

  ~LocalStateMixin() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;

 private:
  Delegate* const delegate_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::LocalStateMixin;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_STATE_MIXIN_H_
