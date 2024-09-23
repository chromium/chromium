// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/soundscapes_downloader.h"

#include "ash/system/focus_mode/sounds/soundscape/test/test_data.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kHost[] = "https://www.example.com";
constexpr char kLocale[] = "en-US";
constexpr char kValidPath[] = "/config.json";

// A simple SharedURLLoaderFactory implementation for tests.
class FakeSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FakeSharedURLLoaderFactory() = default;
  FakeSharedURLLoaderFactory(const FakeSharedURLLoaderFactory&) = delete;
  FakeSharedURLLoaderFactory& operator=(const FakeSharedURLLoaderFactory&) =
      delete;

  // network::mojom::URLLoaderFactory:
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    test_url_loader_factory_.Clone(std::move(receiver));
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    test_url_loader_factory_.CreateLoaderAndStart(
        std::move(loader), request_id, options, request, std::move(client),
        traffic_annotation);
  }

  // network::SharedURLLoaderFactory:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  ~FakeSharedURLLoaderFactory() override = default;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

class SoundscapesDownloaderTest : public testing::Test {
 public:
  SoundscapesDownloaderTest() = default;

  void SetUp() override {
    fake_url_loader_factory_ =
        base::MakeRefCounted<FakeSharedURLLoaderFactory>();
  }

  SoundscapesDownloader::Urls MakeConfiguration(std::string_view path) {
    SoundscapesDownloader::Urls config;
    config.locale = std::string(kLocale);
    config.host = GURL(kHost);
    config.config_path = path;
    return config;
  }

  void ServeConfigOk(GURL url,
                     network::TestURLLoaderFactory& test_url_loader_factory) {
    test_url_loader_factory.SimulateResponseForPendingRequest(
        url.spec(), kTestConfig, net::HTTP_OK);
  }

  void ServeConfigFailed(GURL url,
                         network::TestURLLoaderFactory& test_url_loader_factory,
                         net::HttpStatusCode error) {
    test_url_loader_factory.SimulateResponseForPendingRequest(url.spec(), "",
                                                              error);
  }

  scoped_refptr<FakeSharedURLLoaderFactory> fake_url_loader_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC};
};

TEST_F(SoundscapesDownloaderTest, Construct) {
  auto downloader = SoundscapesDownloader::CreateForTesting(
      MakeConfiguration(""), fake_url_loader_factory_);
  EXPECT_TRUE(downloader);
}

TEST_F(SoundscapesDownloaderTest, FetchFailureNoRetry) {
  auto downloader = SoundscapesDownloader::CreateForTesting(
      MakeConfiguration(kValidPath), fake_url_loader_factory_);

  bool result = true;
  SoundscapeConfiguration config;

  base::RunLoop run_loop;
  downloader->FetchConfiguration(base::BindLambdaForTesting(
      [&](std::optional<SoundscapeConfiguration> configuration) {
        result = !!configuration;
        if (result) {
          config = std::move(*configuration);
        }
        run_loop.Quit();
      }));

  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_NOT_FOUND);

  run_loop.Run();

  EXPECT_FALSE(result);
}

TEST_F(SoundscapesDownloaderTest, FetchFailureWithFailedRetry) {
  auto downloader = SoundscapesDownloader::CreateForTesting(
      MakeConfiguration(kValidPath), fake_url_loader_factory_);

  bool result = true;
  SoundscapeConfiguration config;

  base::RunLoop run_loop;
  downloader->FetchConfiguration(base::BindLambdaForTesting(
      [&](std::optional<SoundscapeConfiguration> configuration) {
        result = !!configuration;
        if (result) {
          config = std::move(*configuration);
        }
        run_loop.Quit();
      }));

  // Initial request: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry #1: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry #2: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry #3: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  run_loop.Run();
  EXPECT_FALSE(result);
}

TEST_F(SoundscapesDownloaderTest, FetchFailureWithSuccessfulRetry) {
  auto downloader = SoundscapesDownloader::CreateForTesting(
      MakeConfiguration(kValidPath), fake_url_loader_factory_);

  bool result = true;
  SoundscapeConfiguration config;

  base::RunLoop run_loop;
  downloader->FetchConfiguration(base::BindLambdaForTesting(
      [&](std::optional<SoundscapeConfiguration> configuration) {
        result = !!configuration;
        if (result) {
          config = std::move(*configuration);
        }
        run_loop.Quit();
      }));

  // Initial request: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry #1: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry #2: failed with 500.
  ServeConfigFailed(GURL(kHost).Resolve(kValidPath),
                    fake_url_loader_factory_->test_url_loader_factory(),
                    net::HTTP_INTERNAL_SERVER_ERROR);

  // Retry #3: succeeded.
  ServeConfigOk(GURL(kHost).Resolve(kValidPath),
                fake_url_loader_factory_->test_url_loader_factory());

  run_loop.Run();
  EXPECT_TRUE(result);

  ASSERT_THAT(config.playlists, testing::SizeIs(4));
  EXPECT_THAT(config.playlists[2],
              testing::Field(&SoundscapePlaylist::name, testing::Eq("Flow")));
  EXPECT_THAT(config.playlists[2],
              testing::Field(&SoundscapePlaylist::tracks, testing::SizeIs(2)));
}

TEST_F(SoundscapesDownloaderTest, FetchSuccess) {
  auto downloader = SoundscapesDownloader::CreateForTesting(
      MakeConfiguration(kValidPath), fake_url_loader_factory_);

  bool result = false;
  SoundscapeConfiguration config;
  base::RunLoop run_loop;
  downloader->FetchConfiguration(base::BindLambdaForTesting(
      [&](std::optional<SoundscapeConfiguration> configuration) {
        result = !!configuration;
        if (result) {
          config = std::move(*configuration);
        }
        run_loop.Quit();
      }));

  ServeConfigOk(GURL(kHost).Resolve(kValidPath),
                fake_url_loader_factory_->test_url_loader_factory());

  run_loop.Run();
  EXPECT_TRUE(result);

  ASSERT_THAT(config.playlists, testing::SizeIs(4));
  EXPECT_THAT(config.playlists[2],
              testing::Field(&SoundscapePlaylist::name, testing::Eq("Flow")));
  EXPECT_THAT(config.playlists[2],
              testing::Field(&SoundscapePlaylist::tracks, testing::SizeIs(2)));
}

}  // namespace
}  // namespace ash
