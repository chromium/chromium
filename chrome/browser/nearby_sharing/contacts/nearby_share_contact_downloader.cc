// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"

#include <utility>

#include "base/check.h"

NearbyShareContactDownloader::NearbyShareContactDownloader(
    const std::string& device_id,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : device_id_(device_id),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {}

NearbyShareContactDownloader::~NearbyShareContactDownloader() = default;

void NearbyShareContactDownloader::Run() {
  CHECK(!was_run_, base::NotFatalUntil::M160);
  was_run_ = true;

  OnRun();
}

void NearbyShareContactDownloader::Succeed(
    std::vector<nearby::sharing::proto::ContactRecord> contacts,
    uint32_t num_unreachable_contacts_filtered_out) {
  CHECK(was_run_, base::NotFatalUntil::M160);
  CHECK(success_callback_, base::NotFatalUntil::M160);

  std::move(success_callback_)
      .Run(std::move(contacts), num_unreachable_contacts_filtered_out);
}

void NearbyShareContactDownloader::Fail() {
  CHECK(was_run_, base::NotFatalUntil::M160);
  CHECK(failure_callback_, base::NotFatalUntil::M160);

  std::move(failure_callback_).Run();
}
