// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_IMPL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_IMPL_H_

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/arc/recommend_apps_fetcher.h"

namespace base {
class Value;
}  // namespace base

namespace network {
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom

class SimpleURLLoader;
}  // namespace network

namespace app_list {

class RecommendAppsFetcherDelegate;

// This class handles the network request for the Arc App Reinstall
// Recommendation. The request requires the X-DFE-Device-Id (Android ID) header.
class RecommendAppsFetcherImpl : public RecommendAppsFetcher {
 public:
  RecommendAppsFetcherImpl(
      RecommendAppsFetcherDelegate* delegate,
      network::mojom::URLLoaderFactory* url_loader_factory);
  ~RecommendAppsFetcherImpl() override;

  RecommendAppsFetcherImpl(const RecommendAppsFetcherImpl&) = delete;
  RecommendAppsFetcherImpl& operator=(const RecommendAppsFetcherImpl&) = delete;

  // RecommendAppsFetcher:
  void StartDownload() override;

  void SetAndroidIdStatusForTesting(bool status) {
    get_android_id_successfully_ = status;
  }

 private:
  // Abort the attempt to download the recommended app list if it takes too
  // long.
  void OnDownloadTimeout();

  // Called when SimpleURLLoader completes.
  void OnDownloaded(std::unique_ptr<std::string> response_body);

  // If the response is not a valid JSON, return std::nullopt.
  // If the response contains no app, return std::nullopt;
  // The value, if exists, is a list containing:
  // 1. name: the title of the app.
  // 2. package_name: name of the package, for example: com.package.name
  // 3. Possibly an Icon URL.
  // Parses an input string that looks somewhat like this:
  // [
  //    {
  //       "title_":{
  //          "name_":"title of app"
  //       },
  //       "id_":{
  //          "id_":"com.package.name"
  //       },
  //       "icon_":{
  //          "url_":{
  //             "privateDoNotAccessOrElseSafeUrlWrappedValue_":"http://icon_url.com/url"
  //          }
  //       }
  //    },
  //    {
  //       "title_":"title of second app",
  //       "packageName_":"second package name."
  //    }
  // ]

  std::optional<base::Value> ParseResponse(std::string_view response);

  raw_ptr<RecommendAppsFetcherDelegate> delegate_;

  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> app_list_loader_;

  int64_t android_id_ = 0;
  bool get_android_id_successfully_ = false;
  int num_get_android_id_retry_ = 0;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the recommended app list.
  base::OneShotTimer download_timer_;

  base::TimeTicks start_time_;
  base::WeakPtrFactory<RecommendAppsFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ARC_RECOMMEND_APPS_FETCHER_IMPL_H_
