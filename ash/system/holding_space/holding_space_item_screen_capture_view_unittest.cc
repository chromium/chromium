// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"

#include <memory>
#include <set>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/system/holding_space/holding_space_ash_test_base.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns the set of all holding space item types which are screen captures.
std::set<HoldingSpaceItem::Type> GetHoldingSpaceItemScreenCaptureTypes() {
  std::set<HoldingSpaceItem::Type> types;
  if (const HoldingSpaceSection* section =
          GetHoldingSpaceSection(HoldingSpaceSectionId::kScreenCaptures)) {
    for (const HoldingSpaceItem::Type& supported_type :
         section->supported_types) {
      DCHECK(HoldingSpaceItem::IsScreenCaptureType(supported_type));
      types.insert(supported_type);
    }
  }
  DCHECK_GT(types.size(), 0u);
  return types;
}

}  // namespace

// HoldingSpaceItemScreenCaptureViewTest ---------------------------------------

// Base class for tests of `HoldingSpaceItemScreenCaptureView`.
class HoldingSpaceItemScreenCaptureViewTest
    : public HoldingSpaceAshTestBase,
      public testing::WithParamInterface<HoldingSpaceItem::Type> {
 public:
  const HoldingSpaceItem* item() const { return item_.get(); }
  const views::View* view() const { return view_.get(); }

 private:
  // HoldingSpaceAshTestBase:
  void SetUp() override {
    HoldingSpaceAshTestBase::SetUp();

    HoldingSpaceItem::Type type = GetParam();
    ASSERT_TRUE(HoldingSpaceItem::IsScreenCaptureType(type));

    item_ = HoldingSpaceItem::CreateFileBackedItem(
        type,
        HoldingSpaceFile(base::FilePath("file_path"),
                         HoldingSpaceFile::FileSystemType::kTest,
                         GURL("filesystem:file_system_url")),
        base::BindOnce(
            [](HoldingSpaceItem::Type type, const base::FilePath& file_path)
                -> std::unique_ptr<HoldingSpaceImage> {
              return std::make_unique<HoldingSpaceImage>(
                  holding_space_util::GetMaxImageSizeForType(type), file_path,
                  /*async_bitmap_resolver=*/base::DoNothing());
            }));

    delegate_ = std::make_unique<HoldingSpaceViewDelegate>(/*bubble=*/nullptr);
    view_ = std::make_unique<HoldingSpaceItemScreenCaptureView>(delegate_.get(),
                                                                item_.get());
  }

  void TearDown() override {
    view_.reset();
    delegate_.reset();
    item_.reset();

    HoldingSpaceAshTestBase::TearDown();
  }

  std::unique_ptr<HoldingSpaceItem> item_;
  std::unique_ptr<HoldingSpaceViewDelegate> delegate_;
  std::unique_ptr<HoldingSpaceItemScreenCaptureView> view_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceItemScreenCaptureViewTest,
    testing::ValuesIn(GetHoldingSpaceItemScreenCaptureTypes()));

// Tests -----------------------------------------------------------------------

// Verifies absence/presence of overlay icon depending on item type.
TEST_P(HoldingSpaceItemScreenCaptureViewTest, OverlayIcon) {
  const views::View* overlay_icon =
      view()->GetViewByID(kHoldingSpaceScreenCaptureOverlayIconId);

  if (item()->type() == HoldingSpaceItem::Type::kScreenshot) {
    EXPECT_FALSE(overlay_icon);
    return;
  }

  ASSERT_TRUE(overlay_icon);

  auto* color_provider_source =
      ColorUtil::GetColorProviderSourceForWindow(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(color_provider_source);
  auto* color_provider = color_provider_source->GetColorProvider();
  ASSERT_TRUE(color_provider);

  // NOTE: The below compares rasterized bitmaps instead of directly comparing
  // image models due to the fact that vector icons have different memory
  // addresses in production code than in test code, resulting in inequality.
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *views::AsViewClass<views::ImageView>(overlay_icon)
           ->GetImageModel()
           .Rasterize(color_provider)
           .bitmap(),
      *ui::ImageModel::FromVectorIcon(
           item()->type() == HoldingSpaceItem::Type::kScreenRecordingGif
               ? kGifIcon
               : vector_icons::kPlayArrowIcon,
           kColorAshButtonIconColor, kHoldingSpaceIconSize)
           .Rasterize(color_provider)
           .bitmap()));
}

}  // namespace ash
