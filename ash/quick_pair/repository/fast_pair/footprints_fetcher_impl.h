// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_IMPL_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_IMPL_H_

#include <optional>

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/footprints_fetcher.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace ash {
namespace quick_pair {

class HttpFetcher;
class FastPairHttpResult;

class FootprintsFetcherImpl : public FootprintsFetcher {
 public:
  FootprintsFetcherImpl();

  FootprintsFetcherImpl(const FootprintsFetcherImpl&) = delete;
  FootprintsFetcherImpl& operator=(const FootprintsFetcherImpl&) = delete;
  ~FootprintsFetcherImpl() override;

  void GetUserDevices(UserReadDevicesCallback callback) override;
  void AddUserFastPairInfo(nearby::fastpair::FastPairInfo info,
                           AddDeviceCallback callback) override;
  void DeleteUserDevice(const std::string& hex_account_key,
                        DeleteDeviceCallback callback) override;

 private:
  void OnGetComplete(UserReadDevicesCallback callback,
                     std::unique_ptr<HttpFetcher> http_fetcher,
                     std::unique_ptr<std::string> response_body,
                     std::unique_ptr<FastPairHttpResult> http_result);

  void OnPostComplete(AddDeviceCallback callback,
                      std::unique_ptr<HttpFetcher> http_fetcher,
                      std::unique_ptr<std::string> response_body,
                      std::unique_ptr<FastPairHttpResult> http_result);

  void OnDeleteComplete(DeleteDeviceCallback callback,
                        std::unique_ptr<HttpFetcher> http_fetcher,
                        std::unique_ptr<std::string> response_body,
                        std::unique_ptr<FastPairHttpResult> http_result);

  base::WeakPtrFactory<FootprintsFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_FOOTPRINTS_FETCHER_IMPL_H_
