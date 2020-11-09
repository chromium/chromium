// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_helper.h"

#include "ash/ambient/test/test_ambient_client.h"
#include "ash/public/cpp/test/test_image_downloader.h"

namespace ash {

AmbientAshTestHelper::AmbientAshTestHelper() {
  image_downloader_ = std::make_unique<TestImageDownloader>();
  ambient_client_ = std::make_unique<TestAmbientClient>(&wake_lock_provider_);
}

AmbientAshTestHelper::~AmbientAshTestHelper() = default;

void AmbientAshTestHelper::IssueAccessToken(const std::string& token,
                                            bool with_error) {
  ambient_client_->IssueAccessToken(token, with_error);
}

bool AmbientAshTestHelper::IsAccessTokenRequestPending() const {
  return ambient_client_->IsAccessTokenRequestPending();
}

}  // namespace ash
