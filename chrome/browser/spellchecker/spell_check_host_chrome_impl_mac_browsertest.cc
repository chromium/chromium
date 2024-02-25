// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

class SpellCheckHostChromeImplMacBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::BrowserContext* context = browser()->profile();
    renderer_ = std::make_unique<content::MockRenderProcessHost>(context);
    SpellCheckHostChromeImpl::Create(
        renderer_->GetID(), spell_check_host_.BindNewPipeAndPassReceiver());
  }

  void TearDownOnMainThread() override { renderer_.reset(); }

  void LogResult(const std::vector<SpellCheckResult>& result) {
    received_result_ = true;
    result_ = result;
    if (quit_)
      std::move(quit_).Run();
  }

  void RunUntilResultReceived() {
    if (received_result_)
      return;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  std::unique_ptr<content::MockRenderProcessHost> renderer_;
  mojo::Remote<spellcheck::mojom::SpellCheckHost> spell_check_host_;

  bool received_result_ = false;
  std::vector<SpellCheckResult> result_;
  base::OnceClosure quit_;
};

// Uses browsertest to setup chrome threads.
IN_PROC_BROWSER_TEST_F(SpellCheckHostChromeImplMacBrowserTest,
                       SpellCheckReturnMessage) {
  spell_check_host_->RequestTextCheck(
      u"zz.", base::BindOnce(&SpellCheckHostChromeImplMacBrowserTest::LogResult,
                             base::Unretained(this)));
  RunUntilResultReceived();

  ASSERT_EQ(1U, result_.size());
  EXPECT_EQ(result_[0].location, 0);
  EXPECT_EQ(result_[0].length, 2);
  EXPECT_EQ(result_[0].decoration, SpellCheckResult::SPELLING);
}
