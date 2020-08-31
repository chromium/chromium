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

namespace kaleidoscope {

namespace {
class GetCollectionsRequest;
}  // namespace

class KaleidoscopeService : public KeyedService {
 public:
  explicit KaleidoscopeService(Profile* profile);
  ~KaleidoscopeService() override;
  KaleidoscopeService(const KaleidoscopeService&) = delete;
  KaleidoscopeService& operator=(const KaleidoscopeService&) = delete;

  static bool IsEnabled();

  // Returns the instance attached to the given |profile|.
  static KaleidoscopeService* Get(Profile* profile);

  using GetCollectionsCallback = base::OnceCallback<void(const std::string&)>;
  void GetCollections(media::mojom::CredentialsPtr credentials,
                      const std::string& gaia_id,
                      const std::string& request,
                      GetCollectionsCallback callback);

  void SetCollectionsForTesting(const std::string& collections);

 private:
  friend class KaleidoscopeServiceTest;

  void OnURLFetchComplete(std::unique_ptr<std::string> data);

  scoped_refptr<::network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForFetcher();

  scoped_refptr<::network::SharedURLLoaderFactory>
      test_url_loader_factory_for_fetcher_;

  Profile* const profile_;

  std::unique_ptr<GetCollectionsRequest> request_;

  std::vector<GetCollectionsCallback> pending_callbacks_;

  base::Optional<std::string> collections_for_testing_;
};

}  // namespace kaleidoscope

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_H_
