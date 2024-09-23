// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/soundscapes_downloader.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ash {

namespace {

constexpr size_t kMaxDownloadBytes = 20 * 1024;

// Max number of retries for fetching the configuration.
constexpr int kMaxRetries = 3;

SoundscapesDownloader::Urls ProductionConfiguration(const std::string& locale) {
  SoundscapesDownloader::Urls configuration;
  configuration.locale = locale;
  configuration.host = GURL("https://gstatic.com/chromeos-focusmode/");
  configuration.config_path = "config.json";
  return configuration;
}

constexpr net::NetworkTrafficAnnotationTag kFocusSoundsNetworkTag =
    net::DefineNetworkTrafficAnnotation("focus_sounds_configuration", R"(
        semantics {
          sender: "Focus Sounds"
          description:
            "Retrieve the list of playlists, songs, and thumbnails for Focus"
            "Sounds in Focus Mode. Songs may be played while a user is in"
            "Focus Mode. Thumbnails may appear in the Focus Mode panel and in"
            "Media Controls."
          trigger:
            "The Focus Mode panel is opened"
          data: "None"
          user_data {
            type: NONE
          }
          internal {
            contacts {
              email: "focusmode-wmp@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-04-26"
        }
        policy {
         cookies_allowed: NO
         setting:
           "Cannot be disabled via settings."
         chrome_policy {
           FocusModeSoundsEnabled {
             FocusModeSoundsEnabled: "disabled"
           }
         }
        })");

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const GURL& url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kFocusSoundsNetworkTag);
}

class SoundscapesDownloaderImpl : public SoundscapesDownloader {
 public:
  SoundscapesDownloaderImpl(
      SoundscapesDownloader::Urls config,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : configuration_(config), url_loader_factory_(url_loader_factory) {}
  ~SoundscapesDownloaderImpl() override = default;

  void FetchConfiguration(ConfigurationCallback callback) override {
    GURL destination = configuration_.host.Resolve(configuration_.config_path);
    pending_request_ = CreateSimpleURLLoader(destination);
    network::SimpleURLLoader::BodyAsStringCallback handler =
        base::BindOnce(&SoundscapesDownloaderImpl::HandleConfigurationString,
                       weak_factory_.GetWeakPtr(),
                       /*start_time=*/base::Time::Now(), std::move(callback));
    const int retry_mode = network::SimpleURLLoader::RETRY_ON_5XX |
                           network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                           network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
    pending_request_->SetRetryOptions(kMaxRetries, retry_mode);
    pending_request_->DownloadToString(url_loader_factory_.get(),
                                       std::move(handler), kMaxDownloadBytes);
  }

  GURL ResolveUrl(std::string_view path) override {
    GURL resolved = configuration_.host.Resolve(path);
    if (!resolved.is_valid()) {
      // If the path segment results in an invalid url, use an empty one
      // instead to guard against accidental use.
      return GURL();
    }

    return resolved;
  }

 private:
  void HandleConfigurationString(const base::Time start_time,
                                 ConfigurationCallback callback,
                                 std::optional<std::string> response_body) {
    const std::string method = "FocusSounds.FetchConfiguration";
    focus_mode_util::RecordHistogramForApiLatency(
        method, base::Time::Now() - start_time);

    // Move the pending request here so it's deleted when this function ends.
    std::unique_ptr<network::SimpleURLLoader> request =
        std::move(pending_request_);

    if (!response_body || response_body->empty()) {
      focus_mode_util::RecordHistogramForApiResult(method,
                                                   /*successful=*/false);
      std::move(callback).Run(std::nullopt);
      return;
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&SoundscapeConfiguration::ParseConfiguration,
                       configuration_.locale, *response_body),
        base::BindOnce(&SoundscapesDownloaderImpl::HandleParsedConfiguration,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void HandleParsedConfiguration(
      ConfigurationCallback callback,
      std::optional<SoundscapeConfiguration> configuration) {
    focus_mode_util::RecordHistogramForApiResult(
        "FocusSounds.FetchConfiguration",
        /*successful=*/configuration.has_value() &&
            !configuration.value().playlists.empty());
    std::move(callback).Run(std::move(configuration));
  }

  std::unique_ptr<network::SimpleURLLoader> pending_request_;
  SoundscapesDownloader::Urls configuration_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<SoundscapesDownloaderImpl> weak_factory_{this};
};

}  // namespace

SoundscapesDownloader::Urls::Urls() = default;
SoundscapesDownloader::Urls::Urls(const Urls&) = default;
SoundscapesDownloader::Urls::~Urls() = default;

// static
std::unique_ptr<SoundscapesDownloader> SoundscapesDownloader::Create(
    const std::string& locale) {
  CHECK(!locale.empty());

  return std::make_unique<SoundscapesDownloaderImpl>(
      ProductionConfiguration(locale),
      Shell::Get()->shell_delegate()->GetBrowserProcessUrlLoaderFactory());
}

// static
std::unique_ptr<SoundscapesDownloader> SoundscapesDownloader::CreateForTesting(
    const SoundscapesDownloader::Urls& configuration,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<SoundscapesDownloaderImpl>(configuration,
                                                     url_loader_factory);
}

}  // namespace ash
