// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace infobars {

class BrowserInfoBarManagerBrowserTest : public InProcessBrowserTest {
 public:
  BrowserInfoBarManagerBrowserTest() {
    feature_list_.InitAndEnableFeature(kCentralizedInfoBarFramework);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (!BrowserInfoBarManager::From(g_browser_process)) {
      owned_manager_ =
          std::make_unique<BrowserInfoBarManager>(g_browser_process);
    }
  }

  void TearDownOnMainThread() override {
    owned_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BrowserInfoBarManager* manager() {
    return BrowserInfoBarManager::From(g_browser_process);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<BrowserInfoBarManager> owned_manager_;
};

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       RegisterAndShowCurrentTab) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kCurrentTab)
                  .Build();

  manager()->Register(std::move(spec));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);
  ASSERT_EQ(0u, infobar_manager->infobars().size());

  manager()->Show(identifier);

  // Now one infobar should be present.
  EXPECT_EQ(1u, infobar_manager->infobars().size());
  if (infobar_manager->infobars().size() > 0) {
    EXPECT_EQ(identifier,
              infobar_manager->infobars()[0]->delegate()->GetIdentifier());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserInfoBarManagerBrowserTest,
                       RegisterShowAndHideCurrentTab) {
  const auto identifier = InfoBarDelegate::TEST_INFOBAR;
  auto spec = InfoBarSpec::Builder(identifier)
                  .SetMessageText(u"Test Message")
                  .SetScope(InfoBarScope::kCurrentTab)
                  .Build();

  manager()->Register(std::move(spec));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* infobar_manager = ContentInfoBarManager::FromWebContents(web_contents);

  manager()->Show(identifier);
  EXPECT_EQ(1u, infobar_manager->infobars().size());

  manager()->Hide(identifier);
  EXPECT_EQ(0u, infobar_manager->infobars().size());
}

}  // namespace infobars
