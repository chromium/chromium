// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"

#include <utility>

#include "base/check.h"

namespace {

void RecordSuccessMetrics(
    const std::vector<nearbyshare::proto::ContactRecord>& contacts) {
  // TODO(https://crbug.com/1105579): Record a success/failure histogram value.
  // TODO(https://crbug.com/1105579): Record total number of contacts returned.
}

void RecordFailureMetrics() {
  // TODO(https://crbug.com/1105579): Record a success/failure histogram value.
}

}  // namespace

NearbyShareContactDownloader::NearbyShareContactDownloader(
    const std::string& device_id,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : device_id_(device_id),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {}

NearbyShareContactDownloader::~NearbyShareContactDownloader() = default;

void NearbyShareContactDownloader::Run() {
  DCHECK(!was_run_);
  was_run_ = true;

  OnRun();
}

void NearbyShareContactDownloader::Succeed(
    std::vector<nearbyshare::proto::ContactRecord> contacts) {
  DCHECK(was_run_);
  DCHECK(success_callback_);
  RecordSuccessMetrics(contacts);

  std::move(success_callback_).Run(std::move(contacts));
}

void NearbyShareContactDownloader::Fail() {
  DCHECK(was_run_);
  DCHECK(failure_callback_);
  RecordFailureMetrics();

  std::move(failure_callback_).Run();
}
