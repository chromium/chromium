// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_STATE_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_STATE_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace ash {

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
  const raw_ptr<Delegate> delegate_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_STATE_MIXIN_H_
