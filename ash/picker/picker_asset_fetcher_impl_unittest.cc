// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_asset_fetcher_impl.h"

#include <utility>

#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl_delegate.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::FieldsAre;

class MockPickerAssetFetcherDelegate : public PickerAssetFetcherImplDelegate {
 public:
  MOCK_METHOD(void,
              FetchFileThumbnail,
              (const base::FilePath& path,
               const gfx::Size& size,
               FetchFileThumbnailCallback callback),
              (override));
};

class PickerAssetFetcherImplTest : public testing::Test {
 public:
  PickerAssetFetcherImplTest() = default;
  PickerAssetFetcherImplTest(const PickerAssetFetcherImplTest&) = delete;
  PickerAssetFetcherImplTest& operator=(const PickerAssetFetcherImplTest&) =
      delete;
  ~PickerAssetFetcherImplTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment environment_;
};

TEST_F(PickerAssetFetcherImplTest, ForwardsToDelegateToFetchThumbnail) {
  MockPickerAssetFetcherDelegate mock_delegate;
  base::test::TestFuture<base::FilePath, gfx::Size,
                         PickerAssetFetcher::FetchFileThumbnailCallback>
      future;
  EXPECT_CALL(mock_delegate, FetchFileThumbnail)
      .WillOnce([&](const base::FilePath& path, const gfx::Size& size,
                    PickerAssetFetcher::FetchFileThumbnailCallback callback) {
        future.SetValue(path, size, std::move(callback));
      });
  PickerAssetFetcherImpl asset_fetcher(&mock_delegate);

  const base::FilePath kPath("test/image.png");
  constexpr gfx::Size kThumbnailSize(10, 20);
  base::test::TestFuture<const SkBitmap*, base::File::Error> callback_future;
  asset_fetcher.FetchFileThumbnail(kPath, kThumbnailSize,
                                   callback_future.GetCallback());

  auto [path, size, callback] = future.Take();
  EXPECT_EQ(path, kPath);
  EXPECT_EQ(size, kThumbnailSize);
  EXPECT_FALSE(callback_future.IsReady());
  const SkBitmap* kBitmap = nullptr;
  const base::File::Error kError = base::File::Error::FILE_ERROR_FAILED;
  std::move(callback).Run(kBitmap, kError);
  EXPECT_THAT(callback_future.Take(), FieldsAre(kBitmap, kError));
}

}  // namespace
}  // namespace ash
