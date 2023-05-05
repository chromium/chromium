// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/file_preview/file_preview_factory.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

// `FilePreviewFactoryTest` includes tests that confirm `FilePreviewFactory` is
// managing the lifetime of its registry correctly. For example, making sure
// entries are removed from the registry once they are no longer valid, to
// prevent use-after-free crashes.
class FilePreviewFactoryTest : public testing::Test {
 public:
  // This forwarding method only needs to exist while
  // `FilePreviewFactory::CreateImageModel()` is a private method.
  ui::ImageModel CreateImageModel(const base::FilePath& path,
                                  const gfx::Size& size) {
    return FilePreviewFactory::Get()->CreateImageModel(path, size);
  }
};

TEST_F(FilePreviewFactoryTest, ControllerRemovedOnModelDestruction) {
  {
    auto model = CreateImageModel(base::FilePath(), gfx::Size(1, 1));
    EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 1u);
    EXPECT_NE(FilePreviewFactory::Get()->GetController(model), nullptr);
  }
  EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 0u);
}

TEST_F(FilePreviewFactoryTest, CopiedImageModelsKeepControllerAlive) {
  ui::ImageModel model_copy;
  {
    auto model = CreateImageModel(base::FilePath(), gfx::Size(1, 1));
    EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 1u);
    model_copy = model;
    EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 1u);
    const auto* controller = FilePreviewFactory::Get()->GetController(model);
    const auto* copy_controller =
        FilePreviewFactory::Get()->GetController(model_copy);
    EXPECT_EQ(controller, copy_controller);
  }

  EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 1u);
  EXPECT_NE(FilePreviewFactory::Get()->GetController(model_copy), nullptr);
}

TEST_F(FilePreviewFactoryTest, CopiedImageSkiasDoNotKeepControllerAlive) {
  ui::ImageModel non_copied_model;
  {
    ui::ColorProvider color_provider;
    auto model = CreateImageModel(base::FilePath(), gfx::Size(1, 1));

    // Keep the `gfx::ImageSkia` in a simple `ui::ImageModel::ImageGenerator`
    // so that it can be passed to `GetController()` to confirm that the
    // associated `FilePreviewController*` is not in the registry after the
    // destruction of `model`.
    non_copied_model = ui::ImageModel::FromImageGenerator(
        base::BindLambdaForTesting(
            [image_skia = model.Rasterize(&color_provider)](
                const ui::ColorProvider*) { return image_skia; }),
        gfx::Size(1, 1));

    EXPECT_EQ(FilePreviewFactory::Get()->GetController(model),
              FilePreviewFactory::Get()->GetController(non_copied_model));
    EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 1u);
  }
  EXPECT_EQ(FilePreviewFactory::Get()->GetController(non_copied_model),
            nullptr);
  EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 0u);
}

TEST_F(FilePreviewFactoryTest, NonFilePreviewImageModel) {
  // We test both generator and non-generator models here, because the
  // internal logic is slightly different, i.e. we exit early if the model isn't
  // an `ImageGenerator`.
  auto generator_model = ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](const ui::ColorProvider*) { return gfx::ImageSkia(); }),
      gfx::Size(1, 1));
  auto image_model = ui::ImageModel::FromImage(gfx::Image());
  EXPECT_EQ(FilePreviewFactory::Get()->GetRegistryForTest().size(), 0u);

  EXPECT_EQ(FilePreviewFactory::Get()->GetController(generator_model), nullptr);
  EXPECT_EQ(FilePreviewFactory::Get()->GetController(image_model), nullptr);
}

}  // namespace ash
