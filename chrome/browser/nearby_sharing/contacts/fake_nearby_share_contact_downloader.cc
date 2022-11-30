// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/fake_nearby_share_contact_downloader.h"

#include <utility>

FakeNearbyShareContactDownloader::Factory::Factory() = default;

FakeNearbyShareContactDownloader::Factory::~Factory() = default;

std::unique_ptr<NearbyShareContactDownloader>
FakeNearbyShareContactDownloader::Factory::CreateInstance(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory,
    SuccessCallback success_callback,
    FailureCallback failure_callback) {
  latest_timeout_ = timeout;
  latest_client_factory_ = client_factory;

  auto instance = std::make_unique<FakeNearbyShareContactDownloader>(
      device_id, std::move(success_callback), std::move(failure_callback));
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareContactDownloader::FakeNearbyShareContactDownloader(
    const std::string& device_id,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : NearbyShareContactDownloader(device_id,
                                   std::move(success_callback),
                                   std::move(failure_callback)) {}

FakeNearbyShareContactDownloader::~FakeNearbyShareContactDownloader() = default;

void FakeNearbyShareContactDownloader::OnRun() {}
