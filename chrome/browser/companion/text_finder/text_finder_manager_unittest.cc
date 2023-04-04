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

TEST_F(TextFinderManagerTest, TextFinderTest) {
  // Set up a text finder manager bound to the mock agent container.
  MockAnnotationAgentContainer mock_agent_container;
  TextFinderManager* manager =
      CreateTextFinderManagerForTest(&mock_agent_container);

  // Create a new text finder.
  const std::string text_directive = "ab,cd";
  base::OnceCallback<void(bool)> finished_finding_callback =
      base::BindOnce([](bool is_found) { return; });
  const auto id = manager->CreateTextFinder(
      text_directive, std::move(finished_finding_callback));
  EXPECT_EQ(manager->Size(), 1u);

  // Remove text finder.
  manager->RemoveTextFinder(id);
  EXPECT_EQ(manager->Size(), 0u);
}

}  // namespace companion
