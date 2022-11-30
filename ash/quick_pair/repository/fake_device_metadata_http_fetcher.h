// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAKE_DEVICE_METADATA_HTTP_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAKE_DEVICE_METADATA_HTTP_FETCHER_H_

#include "ash/quick_pair/repository/http_fetcher.h"

namespace ash {
namespace quick_pair {

class FakeDeviceMetadataHttpFetcher : public HttpFetcher {
 public:
  FakeDeviceMetadataHttpFetcher();
  FakeDeviceMetadataHttpFetcher(const FakeDeviceMetadataHttpFetcher&) = delete;
  FakeDeviceMetadataHttpFetcher& operator=(
      const FakeDeviceMetadataHttpFetcher&) = delete;
  ~FakeDeviceMetadataHttpFetcher() override;

  // Performs a GET request to the desired URL and returns the response, if
  // available, as a string to the provided |callback|.
  void ExecuteGetRequest(const GURL& url,
                         FetchCompleteCallback callback) override;

  int num_gets() { return num_gets_; }
  void set_network_error(bool error) { has_network_error_ = error; }

 private:
  int num_gets_ = 0;
  bool has_network_error_ = false;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAKE_DEVICE_METADATA_HTTP_FETCHER_H_
