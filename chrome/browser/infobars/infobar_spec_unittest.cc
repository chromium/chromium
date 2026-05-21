// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_spec.h"

#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace infobars {

class InfoBarSpecTest : public testing::Test {};

TEST_F(InfoBarSpecTest, BuildDefaultSpec) {
  InfoBarSpec spec =
      InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR).Build();

  EXPECT_EQ(spec.identifier(), InfoBarDelegate::TEST_INFOBAR);
  EXPECT_EQ(spec.priority(), InfoBarPriority::kDefault);
  EXPECT_EQ(spec.scope(), InfoBarScope::kCurrentTab);
  EXPECT_EQ(spec.icon(), nullptr);
  EXPECT_EQ(spec.icon_id(), 0);
  EXPECT_TRUE(spec.expire_on_navigation());
  EXPECT_TRUE(spec.message_text().empty());
  EXPECT_TRUE(spec.link_text().empty());
  EXPECT_TRUE(spec.link_navigation_url().is_empty());
  EXPECT_TRUE(spec.ok_button_label().empty());
  EXPECT_TRUE(spec.ok_button_callback().is_null());
  EXPECT_TRUE(spec.cancel_button_label().empty());
  EXPECT_TRUE(spec.cancel_button_callback().is_null());
  EXPECT_TRUE(spec.dismiss_callback().is_null());
}

TEST_F(InfoBarSpecTest, BuildCustomSpec) {
  std::u16string message = u"Test Message";
  std::u16string link = u"Test Link";
  GURL url("http://example.com");
  std::u16string ok_label = u"OK";
  std::u16string cancel_label = u"Cancel";

  bool ok_called = false;
  auto ok_cb = base::BindRepeating(
      [](bool* called, content::WebContents*) { *called = true; }, &ok_called);

  bool cancel_called = false;
  auto cancel_cb = base::BindRepeating(
      [](bool* called, content::WebContents*) { *called = true; },
      &cancel_called);

  bool dismiss_called = false;
  auto dismiss_cb = base::BindRepeating(
      [](bool* called, content::WebContents*) { *called = true; },
      &dismiss_called);

  InfoBarSpec spec = InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR)
                         .SetMessageText(message)
                         .SetLinkText(link)
                         .SetLinkNavigationUrl(url)
                         .SetScope(InfoBarScope::kGlobal)
                         .SetPriority(InfoBarPriority::kHigh)
                         .SetIconId(123)
                         .SetExpireOnNavigation(false)
                         .AddOkButton(ok_label, ok_cb)
                         .AddCancelButton(cancel_label, cancel_cb)
                         .SetDismissAction(dismiss_cb)
                         .Build();

  EXPECT_EQ(spec.identifier(), InfoBarDelegate::TEST_INFOBAR);
  EXPECT_EQ(spec.message_text(), message);
  EXPECT_EQ(spec.link_text(), link);
  EXPECT_EQ(spec.link_navigation_url(), url);
  EXPECT_EQ(spec.scope(), InfoBarScope::kGlobal);
  EXPECT_EQ(spec.priority(), InfoBarPriority::kHigh);
  EXPECT_EQ(spec.icon_id(), 123);
  EXPECT_FALSE(spec.expire_on_navigation());
  EXPECT_EQ(spec.ok_button_label(), ok_label);
  EXPECT_FALSE(spec.ok_button_callback().is_null());
  EXPECT_EQ(spec.cancel_button_label(), cancel_label);
  EXPECT_FALSE(spec.cancel_button_callback().is_null());
  EXPECT_FALSE(spec.dismiss_callback().is_null());

  spec.ok_button_callback().Run(nullptr);
  EXPECT_TRUE(ok_called);

  spec.cancel_button_callback().Run(nullptr);
  EXPECT_TRUE(cancel_called);

  spec.dismiss_callback().Run(nullptr);
  EXPECT_TRUE(dismiss_called);
}

}  // namespace infobars
