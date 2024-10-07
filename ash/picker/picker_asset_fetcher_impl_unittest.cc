// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_asset_fetcher_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl_delegate.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/test/ash_test_util.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::FieldsAre;
using ::testing::Return;

class MockPickerAssetUrlLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  MockPickerAssetUrlLoaderFactory() = default;
  MockPickerAssetUrlLoaderFactory(const MockPickerAssetUrlLoaderFactory&) =
      delete;
  MockPickerAssetUrlLoaderFactory& operator=(
      const MockPickerAssetUrlLoaderFactory&) = delete;

  // network::SharedURLLoaderFactory
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    test_url_loader_factory_.CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  void AddResponse(const GURL& url,
                   const std::string& content,
                   net::HttpStatusCode status) {
    test_url_loader_factory_.AddResponse(url.spec(), content, status);
  }

 protected:
  ~MockPickerAssetUrlLoaderFactory() override = default;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

class MockPickerAssetFetcherDelegate : public PickerAssetFetcherImplDelegate {
 public:
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetSharedURLLoaderFactory,
              (),
              (override));
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
  InProcessDataDecoder decoder_;
};

TEST_F(PickerAssetFetcherImplTest, FetchGifReturnsEmptyOnFailedNetworkRequest) {
  scoped_refptr<MockPickerAssetUrlLoaderFactory> url_loader_factory =
      base::MakeRefCounted<MockPickerAssetUrlLoaderFactory>();
  const GURL kGifUrl("https://media.tenor.com/invalid-gif.gif");
  url_loader_factory->AddResponse(kGifUrl, "", net::HTTP_NOT_FOUND);
  MockPickerAssetFetcherDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetSharedURLLoaderFactory)
      .WillRepeatedly(Return(url_loader_factory));
  PickerAssetFetcherImpl asset_fetcher(&mock_delegate);

  base::test::TestFuture<std::vector<image_util::AnimationFrame>> future;
  asset_fetcher.FetchGifFromUrl(kGifUrl, future.GetCallback());

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(PickerAssetFetcherImplTest, FetchesGifPreviewImageFromTenorUrl) {
  scoped_refptr<MockPickerAssetUrlLoaderFactory> url_loader_factory =
      base::MakeRefCounted<MockPickerAssetUrlLoaderFactory>();
  const GURL kGifPreviewImageUrl(
      "https://media.tenor.com/gif-image-preview.png");
  constexpr gfx::Size kGifPreviewImageDimensions(10, 20);
  url_loader_factory->AddResponse(
      kGifPreviewImageUrl,
      CreateEncodedImageForTesting(kGifPreviewImageDimensions), net::HTTP_OK);
  MockPickerAssetFetcherDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetSharedURLLoaderFactory)
      .WillRepeatedly(Return(url_loader_factory));
  PickerAssetFetcherImpl asset_fetcher(&mock_delegate);

  base::test::TestFuture<const gfx::ImageSkia&> future;
  asset_fetcher.FetchGifPreviewImageFromUrl(kGifPreviewImageUrl,
                                            future.GetCallback());

  EXPECT_FALSE(future.Get().isNull());
  EXPECT_EQ(future.Get().size(), kGifPreviewImageDimensions);
}

TEST_F(PickerAssetFetcherImplTest, DoesNotFetchGifPreviewImageFromNonTenorUrl) {
  scoped_refptr<MockPickerAssetUrlLoaderFactory> url_loader_factory =
      base::MakeRefCounted<MockPickerAssetUrlLoaderFactory>();
  const GURL kNonTenorUrl("https://media.nottenor.com/gif-image-preview.png");
  url_loader_factory->AddResponse(
      kNonTenorUrl, CreateEncodedImageForTesting(gfx::Size(10, 20)),
      net::HTTP_OK);
  MockPickerAssetFetcherDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetSharedURLLoaderFactory)
      .WillRepeatedly(Return(url_loader_factory));
  PickerAssetFetcherImpl asset_fetcher(&mock_delegate);

  base::test::TestFuture<const gfx::ImageSkia&> future;
  asset_fetcher.FetchGifPreviewImageFromUrl(kNonTenorUrl, future.GetCallback());

  EXPECT_TRUE(future.Get().isNull());
}

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
