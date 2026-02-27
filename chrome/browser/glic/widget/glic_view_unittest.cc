// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_view.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/common/page/drag_operation.h"

namespace glic {

class GlicViewTest : public ChromeViewsTestBase {
 public:
  GlicViewTest() = default;
  ~GlicViewTest() override = default;

  TestingProfile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
};

TEST_F(GlicViewTest, CanDragEnter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicDragAndDropFileUpload);

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  content::DropData drop_data;
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Empty DropData should be rejected.
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with files should be accepted.
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  EXPECT_TRUE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with URL should be accepted.
  drop_data.filenames.clear();
  drop_data.url_infos.emplace_back(GURL("https://example.com"),
                                   std::u16string());
  EXPECT_TRUE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with text should be rejected.
  drop_data.url_infos.clear();
  drop_data.text = u"test text";
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));

  // DropData with html should be rejected.
  drop_data.text.reset();
  drop_data.html = u"<b>test html</b>";
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));
}

TEST_F(GlicViewTest, CanDragEnter_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicDragAndDropFileUpload);

  auto glic_view =
      std::make_unique<GlicView>(profile(), gfx::Size(800, 600), nullptr);

  content::DropData drop_data;
  drop_data.filenames.emplace_back(
      base::FilePath(FILE_PATH_LITERAL("test.txt")),
      base::FilePath(FILE_PATH_LITERAL("test.txt")));
  blink::DragOperationsMask ops = blink::kDragOperationCopy;

  // Should be rejected because the feature is disabled.
  EXPECT_FALSE(glic_view->CanDragEnter(nullptr, drop_data, ops));
}

}  // namespace glic
