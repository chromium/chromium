// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_CLIENT_IDS_DATABASE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_CLIENT_IDS_DATABASE_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/printing/oauth2/status_code.h"

class PrefRegistrySimple;
class GURL;

namespace ash::printing::oauth2 {

// This class is represents a very small and simple database that keeps the list
// of URLs of Authorization Servers that the Client is registered to. Each entry
// must contain the client_id assigned to the Client by corresponding
// Authorization Server.
class ClientIdsDatabase {
 public:
  static std::unique_ptr<ClientIdsDatabase> Create();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  ClientIdsDatabase(const ClientIdsDatabase&) = delete;
  ClientIdsDatabase& operator=(const ClientIdsDatabase&) = delete;
  virtual ~ClientIdsDatabase() = default;

  // Fetch a client_id for given `url`. The result is returned via `callback`.
  // If no errors occur the callback returns StatusCode::kOK and the client_id
  // in the `data` parameter. If the given `url` is not in the database the
  // returned status is still StatusCode::kOK but the `data` is set to an empty
  // string.
  virtual void FetchId(const GURL& url, StatusCallback callback) = 0;

  // Add to the database a new record (`url`, `client_id`). The `url` cannot
  // exist in the database, it is enforced by DCHECK. If DCHECKs are not active
  // and the `url` already exists then the existing record is overwritten.
  // `client_id` cannot be an empty string (also enforced by DCHECK).
  virtual void StoreId(const GURL& url, const std::string& client_id) = 0;

 protected:
  ClientIdsDatabase() = default;
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_CLIENT_IDS_DATABASE_H_
