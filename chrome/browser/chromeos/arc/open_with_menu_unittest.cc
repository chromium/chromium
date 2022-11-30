// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/open_with_menu.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/common/intent_helper/link_handler_model.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace arc {

namespace {

using base::ASCIIToUTF16;

// All tests in this file assume that 4 and 10 IDC command IDs are reserved
// for the main and sub menus, respectively.
const int kFirstMainMenuId = IDC_CONTENT_CONTEXT_OPEN_WITH1;
const int kFirstSubMenuId = kFirstMainMenuId + 4;
const int kNumCommandIds =
    IDC_CONTENT_CONTEXT_OPEN_WITH_LAST - IDC_CONTENT_CONTEXT_OPEN_WITH1 + 1;

static_assert(kNumCommandIds == 14,
              "invalid number of command IDs reserved for open with");

std::vector<LinkHandlerInfo> CreateLinkHandlerInfo(size_t num_apps) {
  std::vector<LinkHandlerInfo> handlers;
  for (size_t i = 0; i < num_apps; ++i) {
    gfx::ImageSkia image_skia;
    image_skia.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
    LinkHandlerInfo info = {
        base::UTF8ToUTF16(base::StringPrintf("App %" PRIuS, i)),
        // Use an empty image for the first item to test ModelChanged() with
        // both empty and non-empty icons.
        (i == 0) ? gfx::Image() : gfx::Image(image_skia),
        static_cast<uint32_t>(i)};
    handlers.push_back(info);
  }
  return handlers;
}

std::pair<OpenWithMenu::HandlerMap, int> BuildHandlersMap(size_t num_apps) {
  return OpenWithMenu::BuildHandlersMapForTesting(
      CreateLinkHandlerInfo(num_apps));
}

std::u16string GetTitle(size_t i) {
  return l10n_util::GetStringFUTF16(
      IDS_CONTENT_CONTEXT_OPEN_WITH_APP,
      base::UTF8ToUTF16(base::StringPrintf("App %" PRIuS, i)));
}

TEST(OpenWithMenuTest, TestBuildHandlersMap) {
  auto result = BuildHandlersMap(0);
  EXPECT_EQ(0U, result.first.size());
  EXPECT_EQ(-1, result.second);

  result = BuildHandlersMap(1);
  ASSERT_EQ(1U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  EXPECT_EQ(-1, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);

  result = BuildHandlersMap(2);
  EXPECT_EQ(2U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(-1, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);

  result = BuildHandlersMap(3);
  EXPECT_EQ(3U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 2));
  EXPECT_EQ(-1, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);
  EXPECT_EQ(u"App 2", result.first[kFirstMainMenuId + 2].name);

  // Test if app names will overflow to the sub menu.
  result = BuildHandlersMap(4);
  EXPECT_EQ(4U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  // In this case, kFirstMainMenuId + 2 should be hidden.
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  ASSERT_EQ(1U, result.first.count(kFirstSubMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstSubMenuId + 1));
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);
  EXPECT_EQ(u"App 2", result.first[kFirstSubMenuId].name);
  EXPECT_EQ(u"App 3", result.first[kFirstSubMenuId + 1].name);

  result = BuildHandlersMap(11);
  EXPECT_EQ(11U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 9; ++i) {
    ASSERT_EQ(1U, result.first.count(kFirstSubMenuId + i)) << i;
  }

  // The main and sub menus can show up to 12 (=3+10-1) app names.
  result = BuildHandlersMap(12);
  EXPECT_EQ(12U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 10; ++i) {
    const int id = kFirstSubMenuId + i;
    ASSERT_EQ(1U, result.first.count(id)) << i;
    EXPECT_EQ(ASCIIToUTF16(base::StringPrintf("App %zu", i + 2)),
              result.first[id].name)
        << i;
  }

  result = BuildHandlersMap(13);
  EXPECT_EQ(12U, result.first.size());  // still 12
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 10; ++i) {  // still 10
    const int id = kFirstSubMenuId + i;
    ASSERT_EQ(1U, result.first.count(id)) << i;
    EXPECT_EQ(ASCIIToUTF16(base::StringPrintf("App %zu", i + 2)),
              result.first[id].name)
        << i;
  }

  result = BuildHandlersMap(1000);
  EXPECT_EQ(12U, result.first.size());  // still 12
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ(u"App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ(u"App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 10; ++i) {  // still 10
    const int id = kFirstSubMenuId + i;
    ASSERT_EQ(1U, result.first.count(id)) << i;
    EXPECT_EQ(ASCIIToUTF16(base::StringPrintf("App %zu", i + 2)),
              result.first[id].name)
        << i;
  }
}

TEST(OpenWithMenuTest, TestModelChanged) {
  content::BrowserTaskEnvironment task_environment;
  MockRenderViewContextMenu mock_menu(false);
  OpenWithMenu observer(nullptr, &mock_menu);
  mock_menu.SetObserver(&observer);

  // Do the initial setup.
  ui::SimpleMenuModel sub_menu(nullptr);
  OpenWithMenu::AddPlaceholderItemsForTesting(&mock_menu, &sub_menu);
  CHECK_EQ(static_cast<size_t>(kNumCommandIds), mock_menu.GetMenuSize());

  // Check that all menu items are hidden when there is no app to show.
  const size_t kZeroApps = 0;
  MockRenderViewContextMenu::MockMenuItem item;
  observer.ModelChanged(CreateLinkHandlerInfo(kZeroApps));
  EXPECT_EQ(static_cast<size_t>(kNumCommandIds), mock_menu.GetMenuSize());
  for (size_t i = 0; i < mock_menu.GetMenuSize(); ++i) {
    EXPECT_TRUE(mock_menu.GetMenuItem(i, &item)) << i;
    EXPECT_TRUE(item.hidden) << i;
  }

  // Check that all 3 apps are on the main menu.
  const size_t kThreeApps = 3;
  observer.ModelChanged(CreateLinkHandlerInfo(kThreeApps));
  EXPECT_EQ(static_cast<size_t>(kNumCommandIds), mock_menu.GetMenuSize());
  for (size_t i = 0; i < kThreeApps; ++i) {
    EXPECT_TRUE(mock_menu.GetMenuItem(i, &item)) << i;
    EXPECT_TRUE(item.enabled) << i;
    EXPECT_FALSE(item.hidden) << i;
    EXPECT_EQ(GetTitle(i), item.title) << i;
    if (i == 0) {
      EXPECT_TRUE(item.icon.IsEmpty());
    } else {
      EXPECT_FALSE(item.icon.IsEmpty());
    }
  }
  for (size_t i = kThreeApps; i < mock_menu.GetMenuSize(); ++i) {
    EXPECT_TRUE(mock_menu.GetMenuItem(i, &item)) << i;
    EXPECT_TRUE(item.hidden) << i;
  }

  // Check that the first 2 apps are on the main menu and the rest is on the
  // submenu.
  const size_t kFourApps = 4;
  observer.ModelChanged(CreateLinkHandlerInfo(kFourApps));
  EXPECT_EQ(static_cast<size_t>(kNumCommandIds), mock_menu.GetMenuSize());
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_TRUE(mock_menu.GetMenuItem(i, &item)) << i;
    EXPECT_TRUE(item.enabled) << i;
    EXPECT_FALSE(item.hidden) << i;
    EXPECT_EQ(GetTitle(i), item.title) << i;
    if (i == 0) {
      EXPECT_TRUE(item.icon.IsEmpty());
    } else {
      EXPECT_FALSE(item.icon.IsEmpty()) << i;
    }
  }
  // The third item on the main menu should be hidden.
  EXPECT_TRUE(mock_menu.GetMenuItem(2, &item));
  EXPECT_TRUE(item.hidden);
  // The parent of the submenu.
  EXPECT_TRUE(mock_menu.GetMenuItem(3, &item));
  EXPECT_TRUE(item.enabled);
  EXPECT_FALSE(item.hidden);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_MORE_APPS),
            item.title);
  for (size_t i = 4; i < 6; ++i) {
    EXPECT_TRUE(mock_menu.GetMenuItem(i, &item)) << i;
    EXPECT_TRUE(item.enabled) << i;
    EXPECT_FALSE(item.hidden) << i;
    EXPECT_EQ(GetTitle(i - 2), item.title) << i;
    EXPECT_FALSE(item.icon.IsEmpty()) << i;
  }
  for (size_t i = 6; i < mock_menu.GetMenuSize(); ++i) {
    EXPECT_TRUE(mock_menu.GetMenuItem(i, &item)) << i;
    EXPECT_TRUE(item.hidden) << i;
  }
}

}  // namespace

}  // namespace arc
