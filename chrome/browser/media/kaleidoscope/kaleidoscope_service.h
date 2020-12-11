// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_H_

#include <memory>
#include <set>

#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class Profile;

namespace base {
class Clock;
}  // namespace base

namespace kaleidoscope {

namespace {
class GetCollectionsRequest;
}  // namespace

class KaleidoscopeService : public KeyedService {
 public:
  static const char kNTPModuleCacheHitHistogramName[];
  static const char kNTPModuleServerFetchTimeHistogramName[];

  // When we try and ger Kaleidoscope data we store whether we hit the cache in
  // |kNTPModuleCacheHitHistogramName|. Do not change the numbering since this
  // is recorded.
  enum class CacheHitResult {
    kCacheHit = 0,
    kCacheMiss = 1,
    kMaxValue = kCacheMiss,
  };

  explicit KaleidoscopeService(Profile* profile);
  ~KaleidoscopeService() override;
  KaleidoscopeService(const KaleidoscopeService&) = delete;
  KaleidoscopeService& operator=(const KaleidoscopeService&) = delete;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static KaleidoscopeService* Get(Profile* profile);

  using GetCollectionsCallback =
      base::OnceCallback<void(media::mojom::GetCollectionsResponsePtr)>;
  void GetCollections(media::mojom::CredentialsPtr credentials,
                      const std::string& gaia_id,
                      const std::string& request,
                      GetCollectionsCallback callback);

  void SetCollectionsForTesting(const std::string& collections);

  bool ShouldShowFirstRunExperience();

 private:
  friend class KaleidoscopeServiceTest;

  void OnGotCachedData(media::mojom::CredentialsPtr credentials,
                       const std::string& gaia_id,
                       const std::string& request,
                       GetCollectionsCallback callback,
                       media::mojom::GetCollectionsResponsePtr cached);

  void OnURLFetchComplete(const std::string& gaia_id, const std::string& data);

  scoped_refptr<::network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForFetcher();

  scoped_refptr<::network::SharedURLLoaderFactory>
      test_url_loader_factory_for_fetcher_;

  Profile* const profile_;

  std::unique_ptr<GetCollectionsRequest> request_;

  std::vector<GetCollectionsCallback> pending_callbacks_;

  base::Clock* clock_;

  base::WeakPtrFactory<KaleidoscopeService> weak_ptr_factory_{this};
};

}  // namespace kaleidoscope

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_H_
