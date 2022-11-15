// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/mock_client_ids_database.h"

#include <utility>

#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

MockClientIdsDatabase::MockClientIdsDatabase() {
  ON_CALL(*this, FetchId)
      .WillByDefault([](const GURL& url, StatusCallback callback) {
        std::move(callback).Run(StatusCode::kOK, "");
      });
}

MockClientIdsDatabase::~MockClientIdsDatabase() = default;

}  // namespace ash::printing::oauth2
