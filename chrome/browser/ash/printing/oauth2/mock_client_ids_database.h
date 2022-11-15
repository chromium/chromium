// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_MOCK_CLIENT_IDS_DATABASE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_MOCK_CLIENT_IDS_DATABASE_H_

#include <string>

#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace ash::printing::oauth2 {

class MockClientIdsDatabase : public ClientIdsDatabase {
 public:
  MockClientIdsDatabase();
  ~MockClientIdsDatabase() override;

  // By default, this method calls the given `callback` with the status
  // StatusCode::kOK and an empty string (which means that `url` is not known).
  MOCK_METHOD(void,
              FetchId,
              (const GURL& url, StatusCallback callback),
              (override));

  MOCK_METHOD(void,
              StoreId,
              (const GURL& url, const std::string& id),
              (override));
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_MOCK_CLIENT_IDS_DATABASE_H_
