// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_IMPL_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/shell_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/device_configuration.pb.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"
#include "chromeos/ash/experiences/arc/arc_features_parser.h"
#include "extensions/browser/api/system_display/display_info_provider.h"

namespace ash {
class CrosDisplayConfig;
class Shell;
}  // namespace ash

namespace gpu {
struct GPUInfo;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}

class SimpleURLLoader;
}  // namespace network

namespace apps {
class RecommendAppsFetcherDelegate;

// This class handles the network request for the Recommend Apps screen. It is
// supposed to run on the UI thread. The request requires the following headers:
// 1. X-Device-Config
// 2. X-Sdk-Version
// Play requires Android device config information to filter apps.
// device_configuration.proto is used to encode all the info. The following
// fields will be retrieved and sent:
// 1. touch_screen
// 2. keyboard
// 3. navigation
// 4. screen_layout
// 5. has_hard_keyboard
// 6. has_five_way_navigation
// 7. screen_density
// 8. screen_width
// 9. screen_height
// 10. gl_es_version
// 11. system_available_feature
// 12. native_platform
// 13. gl_extension
class RecommendAppsFetcherImpl : public RecommendAppsFetcher,
                                 public ash::ShellObserver {
 public:
  class ScopedGpuInfoForTest {
   public:
    explicit ScopedGpuInfoForTest(const gpu::GPUInfo* gpu_info);
    ~ScopedGpuInfoForTest();
  };

  RecommendAppsFetcherImpl(
      RecommendAppsFetcherDelegate* delegate,
      ash::CrosDisplayConfig* cros_display_config,
      network::mojom::URLLoaderFactory* url_loader_factory);

  RecommendAppsFetcherImpl(const RecommendAppsFetcherImpl&) = delete;
  RecommendAppsFetcherImpl& operator=(const RecommendAppsFetcherImpl&) = delete;

  ~RecommendAppsFetcherImpl() override;

  // Provide a retry method to download the app list again.
  // RecommendAppsFetcher:
  void Start() override;
  void Retry() override;

  using ArcFeaturesGetter = base::RepeatingCallback<void(
      base::OnceCallback<void(std::optional<arc::ArcFeatures> callback)>)>;
  void set_arc_features_getter_for_testing(const ArcFeaturesGetter& getter) {
    arc_features_getter_ = getter;
  }

  // ash::ShellObserver:
  void OnShellDestroying() override;

 private:
  // Populate the required device config info.
  void PopulateDeviceConfig();

  // Helper for `PopulateDeviceConfig` that deals with displays.
  void PopulateDisplaySettings();

  // Start to compress and encode the proto message if we finish ash request
  // and ARC feature is read.
  void MaybeStartCompressAndEncodeProtoMessage();

  // Callback function called when ARC features are read by the parser.
  // It will populate the device config info related to ARC features.
  void OnArcFeaturesRead(std::optional<arc::ArcFeatures> read_result);

  // Callback function called when the proto message has been compressed and
  // encoded.
  void OnProtoMessageCompressedAndEncoded(
      std::string encoded_device_configuration_proto);

  // Start downloading the recommended app list.
  void StartDownload();

  // Abort the attempt to download the recommended app list if it takes too
  // long.
  void OnDownloadTimeout();

  // Callback function called when SimpleURLLoader completes.
  void OnDownloaded(std::optional<std::string> response_body);

  device_configuration::DeviceConfigurationProto device_config_;

  std::string android_sdk_version_;

  std::string play_store_version_;

  std::string device_fingerprint_;

  std::string encoded_device_configuration_proto_;

  bool arc_features_ready_ = false;
  bool has_started_proto_processing_ = false;
  bool proto_compressed_and_encoded_ = false;

  raw_ptr<RecommendAppsFetcherDelegate> delegate_;

  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> app_list_loader_;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the recommended app list.
  base::OneShotTimer download_timer_;

  base::TimeTicks start_time_;

  ArcFeaturesGetter arc_features_getter_;

  raw_ptr<ash::CrosDisplayConfig> cros_display_config_;

  // TODO(crbug.com/485123493): Remove the observation and OnShellDestroying
  // override once profiles (and thus RecommendAppsFetcherImpl) get destroyed
  // before ash::Shell.
  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};

  base::WeakPtrFactory<RecommendAppsFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_RECOMMENDED_ARC_APPS_RECOMMEND_APPS_FETCHER_IMPL_H_
