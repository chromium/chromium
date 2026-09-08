// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/scoped_modal_dialog_manager_delegate.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class ScopedModalDialogManagerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ScopedModalDialogManagerDelegateTest() = default;
  ~ScopedModalDialogManagerDelegateTest() override = default;
};

TEST_F(ScopedModalDialogManagerDelegateTest, SetWebContents_SetsDelegate) {
  web_modal::TestWebContentsModalDialogManagerDelegate delegate;
  ScopedModalDialogManagerDelegate scoped_delegate(&delegate);

  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  scoped_delegate.SetWebContents(wc.get());

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(wc.get());
  ASSERT_TRUE(manager);
  EXPECT_EQ(manager->delegate(), &delegate);
}

TEST_F(ScopedModalDialogManagerDelegateTest,
       SetWebContents_ReplacesOldDelegate) {
  web_modal::TestWebContentsModalDialogManagerDelegate delegate;
  ScopedModalDialogManagerDelegate scoped_delegate(&delegate);

  std::unique_ptr<content::WebContents> wc1 = CreateTestWebContents();
  std::unique_ptr<content::WebContents> wc2 = CreateTestWebContents();

  scoped_delegate.SetWebContents(wc1.get());
  auto* manager1 =
      web_modal::WebContentsModalDialogManager::FromWebContents(wc1.get());
  ASSERT_TRUE(manager1);
  EXPECT_EQ(manager1->delegate(), &delegate);

  // Switching to wc2 must clear delegate on wc1 and set it on wc2.
  scoped_delegate.SetWebContents(wc2.get());
  EXPECT_EQ(manager1->delegate(), nullptr);

  auto* manager2 =
      web_modal::WebContentsModalDialogManager::FromWebContents(wc2.get());
  ASSERT_TRUE(manager2);
  EXPECT_EQ(manager2->delegate(), &delegate);
}

TEST_F(ScopedModalDialogManagerDelegateTest,
       SetWebContents_NullClearsDelegate) {
  web_modal::TestWebContentsModalDialogManagerDelegate delegate;
  ScopedModalDialogManagerDelegate scoped_delegate(&delegate);

  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  scoped_delegate.SetWebContents(wc.get());

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(wc.get());
  ASSERT_TRUE(manager);
  EXPECT_EQ(manager->delegate(), &delegate);

  scoped_delegate.SetWebContents(nullptr);
  EXPECT_EQ(manager->delegate(), nullptr);
}

TEST_F(ScopedModalDialogManagerDelegateTest, Destruction_ClearsDelegate) {
  web_modal::TestWebContentsModalDialogManagerDelegate delegate;
  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();

  {
    ScopedModalDialogManagerDelegate scoped_delegate(&delegate);
    scoped_delegate.SetWebContents(wc.get());

    auto* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(wc.get());
    ASSERT_TRUE(manager);
    EXPECT_EQ(manager->delegate(), &delegate);
  }

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(wc.get());
  ASSERT_TRUE(manager);
  EXPECT_EQ(manager->delegate(), nullptr);
}

TEST_F(ScopedModalDialogManagerDelegateTest,
       WebContentsDestroyed_HandlesGracefully) {
  web_modal::TestWebContentsModalDialogManagerDelegate delegate;
  ScopedModalDialogManagerDelegate scoped_delegate(&delegate);

  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  scoped_delegate.SetWebContents(wc.get());

  // Destroying the WebContents should not cause a crash or UAF in
  // scoped_delegate.
  wc.reset();
  // Safe to call SetWebContents again or destruct.
  scoped_delegate.SetWebContents(nullptr);
}

TEST_F(ScopedModalDialogManagerDelegateTest, DoesNotClearDifferentDelegate) {
  web_modal::TestWebContentsModalDialogManagerDelegate delegate1;
  web_modal::TestWebContentsModalDialogManagerDelegate delegate2;

  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();

  {
    ScopedModalDialogManagerDelegate scoped_delegate1(&delegate1);
    scoped_delegate1.SetWebContents(wc.get());

    // If another delegate took over the manager:
    auto* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(wc.get());
    ASSERT_TRUE(manager);
    manager->SetDelegate(&delegate2);

    // Destructing scoped_delegate1 should not overwrite delegate2.
  }

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(wc.get());
  ASSERT_TRUE(manager);
  EXPECT_EQ(manager->delegate(), &delegate2);
}

}  // namespace glic
