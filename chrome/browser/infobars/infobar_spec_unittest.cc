// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_spec.h"

#include <optional>
#include <vector>

#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace infobars {

class InfoBarSpecTest : public testing::Test {};

TEST_F(InfoBarSpecTest, BuildDefaultSpec) {
  InfoBarSpec spec =
      InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR).Build();

  EXPECT_EQ(spec.identifier(), InfoBarDelegate::TEST_INFOBAR);
  EXPECT_EQ(spec.priority(), InfoBarDelegate::InfobarPriority::kDefault);
  EXPECT_EQ(spec.scope(), InfoBarScope::kTab);
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
  EXPECT_EQ(spec.dark_mode_icon(), nullptr);
  EXPECT_TRUE(spec.result_callback().is_null());
  EXPECT_TRUE(spec.browser_filter().is_null());
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

  InfoBarSpec spec =
      InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR)
          .SetMessageText(message)
          .SetLinkText(link)
          .SetLinkNavigationUrl(url)
          .SetScope(InfoBarScope::kGlobal)
          .SetPriority(InfoBarDelegate::InfobarPriority::kCriticalSecurity)
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
  EXPECT_EQ(spec.priority(),
            InfoBarDelegate::InfobarPriority::kCriticalSecurity);
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

TEST_F(InfoBarSpecTest, BuildSpecWithResultCallbackAndBrowserFilter) {
  std::optional<InfoBarResult> reported_result;
  auto result_cb = base::BindRepeating(
      [](std::optional<InfoBarResult>* result, content::WebContents*,
         InfoBarResult reported) { *result = reported; },
      &reported_result);

  bool filter_called = false;
  auto filter_cb = base::BindRepeating(
      [](bool* called, BrowserWindowInterface*) {
        *called = true;
        return false;
      },
      &filter_called);

  InfoBarSpec spec = InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR)
                         .SetResultCallback(result_cb)
                         .SetBrowserFilter(filter_cb)
                         .Build();

  ASSERT_FALSE(spec.result_callback().is_null());
  spec.result_callback().Run(nullptr, InfoBarResult::kAccepted);
  EXPECT_EQ(reported_result, InfoBarResult::kAccepted);

  ASSERT_FALSE(spec.browser_filter().is_null());
  EXPECT_FALSE(spec.browser_filter().Run(nullptr));
  EXPECT_TRUE(filter_called);
}

TEST_F(InfoBarSpecTest, BuildSpecWithTemplateAndSubstitutions) {
  auto substitutions_cb = base::BindRepeating([](content::WebContents*) {
    std::vector<MessageSubstitution> substitutions;
    substitutions.emplace_back(u"link text", /*is_link=*/true,
                               /*accessible_name=*/std::nullopt);
    return substitutions;
  });

  bool link_clicked = false;
  auto link_cb = base::BindRepeating(
      [](bool* clicked, content::WebContents*, size_t index,
         WindowOpenDisposition) {
        *clicked = true;
        return true;
      },
      &link_clicked);

  InfoBarSpec spec = InfoBarSpec::Builder(InfoBarDelegate::TEST_INFOBAR)
                         .SetMessageTextTemplate(u"Open $1 to continue")
                         .SetSubstitutionsCallback(substitutions_cb)
                         .SetInlineLinkCallback(link_cb)
                         .Build();

  EXPECT_EQ(spec.message_text_template(), u"Open $1 to continue");
  ASSERT_FALSE(spec.substitutions_callback().is_null());
  std::vector<MessageSubstitution> substitutions =
      spec.substitutions_callback().Run(nullptr);
  ASSERT_EQ(substitutions.size(), 1u);
  EXPECT_EQ(substitutions[0].text, u"link text");
  EXPECT_TRUE(substitutions[0].is_link);

  ASSERT_FALSE(spec.inline_link_callback().is_null());
  EXPECT_TRUE(spec.inline_link_callback().Run(
      nullptr, 0, WindowOpenDisposition::CURRENT_TAB));
  EXPECT_TRUE(link_clicked);
}

}  // namespace infobars
