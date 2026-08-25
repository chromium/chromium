// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_menu_model.h"

#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"

namespace contextual_cueing {
namespace {

class ContextualCueingMenuModelTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ContextualCueingMenuModelTest, MenuStructureWithEditPrompt) {
  optimization_guide::proto::ContextualCue cue;
  ContextualCueingMenuModel model(
      /*profile=*/nullptr, /*controller=*/nullptr, CueTargetType::kGlic, cue,
      /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"",
      /*data=*/GlicCueActionData(), /*cue_id=*/"test_cue",
      /*supports_edit_prompt=*/true);

  // Expected items:
  // 0: Dismiss (command ID 1)
  // 1: Edit prompt (command ID 2)
  // 2: Separator
  // 3: Settings (command ID 3)
  ASSERT_EQ(model.GetItemCount(), 4u);

  EXPECT_EQ(model.GetCommandIdAt(0), 1);
  EXPECT_EQ(model.GetTypeAt(0), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_CONTEXTUAL_CUEING_MENU_DISMISS));
  EXPECT_FALSE(model.GetIconAt(0).IsEmpty());

  EXPECT_EQ(model.GetCommandIdAt(1), 2);
  EXPECT_EQ(model.GetTypeAt(1), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetLabelAt(1),
            l10n_util::GetStringUTF16(IDS_CONTEXTUAL_CUEING_MENU_EDIT_PROMPT));
  EXPECT_FALSE(model.GetIconAt(1).IsEmpty());

  EXPECT_EQ(model.GetTypeAt(2), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_EQ(model.GetCommandIdAt(3), 3);
  EXPECT_EQ(model.GetTypeAt(3), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetLabelAt(3),
            l10n_util::GetStringUTF16(IDS_CONTEXTUAL_CUEING_MENU_SETTINGS));
  EXPECT_FALSE(model.GetIconAt(3).IsEmpty());
}

TEST_F(ContextualCueingMenuModelTest, MenuStructureWithoutEditPrompt) {
  optimization_guide::proto::ContextualCue cue;
  ContextualCueingMenuModel model(
      /*profile=*/nullptr, /*controller=*/nullptr, CueTargetType::kTestSource,
      cue, /*tabs_to_show=*/{}, /*background_tabs=*/{}, /*cuj=*/"",
      /*data=*/GlicCueActionData(), /*cue_id=*/"test_cue",
      /*supports_edit_prompt=*/false);

  // Expected items:
  // 0: Dismiss (command ID 1)
  // 1: Separator
  // 2: Settings (command ID 3)
  ASSERT_EQ(model.GetItemCount(), 3u);

  EXPECT_EQ(model.GetCommandIdAt(0), 1);
  EXPECT_EQ(model.GetTypeAt(0), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_CONTEXTUAL_CUEING_MENU_DISMISS));
  EXPECT_FALSE(model.GetIconAt(0).IsEmpty());

  EXPECT_EQ(model.GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_EQ(model.GetCommandIdAt(2), 3);
  EXPECT_EQ(model.GetTypeAt(2), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetLabelAt(2),
            l10n_util::GetStringUTF16(IDS_CONTEXTUAL_CUEING_MENU_SETTINGS));
  EXPECT_FALSE(model.GetIconAt(2).IsEmpty());
}

}  // namespace
}  // namespace contextual_cueing
