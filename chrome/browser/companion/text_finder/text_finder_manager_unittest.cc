// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_finder_manager.h"

#include "chrome/browser/companion/text_finder/text_finder.h"
#include "chrome/browser/companion/text_finder/text_finder_manager_base_test.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {

using ::testing::_;

class TextFinderManagerTest : public TextFinderManagerBaseTest {
 public:
  TextFinderManagerTest() = default;
  ~TextFinderManagerTest() override = default;

  void SetUp() override { TextFinderManagerBaseTest::SetUp(); }
};

TEST_F(TextFinderManagerTest, SingleTextFinderTest) {
  // Set up a text finder manager bound to the mock agent container.
  MockAnnotationAgentContainer mock_agent_container;
  TextFinderManager* manager =
      CreateTextFinderManagerForTest(&mock_agent_container);

  // Create a new text finder.
  const std::string text_directive = "ab,cd";
  base::OnceCallback<void(std::pair<std::string, bool>)>
      finished_finding_callback = base::BindOnce(
          [](std::pair<std::string, bool> text_found) { return; });
  const auto id = manager->CreateTextFinder(
      text_directive, std::move(finished_finding_callback));
  EXPECT_TRUE(id.has_value());
  EXPECT_EQ(manager->Size(), 1u);

  // Remove text finder.
  manager->RemoveTextFinder(id.value());
  EXPECT_EQ(manager->Size(), 0u);
}

TEST_F(TextFinderManagerTest, MultiTextFindersTest) {
  // Set up a text finder manager bound to the mock agent container.
  MockAnnotationAgentContainer mock_agent_container;
  TextFinderManager* manager =
      CreateTextFinderManagerForTest(&mock_agent_container);

  // Create multiple text finders.
  const std::vector<std::string> text_directives = {"ab,cd", "def"};
  TextFinderManager::AllDoneCallback all_done_callback = base::BindOnce(
      [](const std::vector<std::pair<std::string, bool>>& text_found) {
        ASSERT_EQ(text_found.size(), 2u);
        // One found, one not found.
        EXPECT_TRUE(text_found[0].second || text_found[1].second);
        EXPECT_FALSE(text_found[0].second && text_found[1].second);
      });
  manager->CreateTextFinders(text_directives, std::move(all_done_callback));
  EXPECT_EQ(manager->Size(), 2u);

  gfx::Rect rect_1(2, 4), rect_2;
  for (const auto& id_and_text_finder : manager->finder_map_) {
    EXPECT_TRUE(id_and_text_finder.second->GetTextDirective() == "ab,cd" ||
                id_and_text_finder.second->GetTextDirective() == "def");
    if (id_and_text_finder.second->GetTextDirective() == "ab,cd") {
      // Found text
      id_and_text_finder.second->DidFinishAttachment(rect_1);
    } else {
      // Not found.
      id_and_text_finder.second->DidFinishAttachment(rect_2);
    }
  }
}

}  // namespace companion
