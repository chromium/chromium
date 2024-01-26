// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_STORAGE_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_STORAGE_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "url/origin.h"

struct OHTTPKeyAndExpiration {
  // The OHTTP key in this struct is formatted as described in
  // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-02.html#name-key-configuration-encoding
  std::string key;  // OHTTP key
  base::Time expiration;
};

struct KeyAndNonUniqueUserId {
  std::string key_commitment;  // trust token key commitment (specific to
                               // `non_unique_user_id`)
  int non_unique_user_id;      // Non-unique ID assigned to this user for k-anon
                               // reporting
};

struct KeyAndNonUniqueUserIdWithExpiration {
  KeyAndNonUniqueUserId key_and_id;
  base::Time expiration;
};

class KAnonymityServiceStorage {
 public:
  enum InitStatus {
    kInitOk,
    kInitError,
    // kInitNotReady is only be used internally, not as a response to
    // WaitUntilReady.
    kInitNotReady,
  };
  virtual ~KAnonymityServiceStorage();
  virtual void WaitUntilReady(
      base::OnceCallback<void(InitStatus)> on_ready) = 0;

  virtual std::optional<OHTTPKeyAndExpiration> GetOHTTPKeyFor(
      const url::Origin& origin) const = 0;
  virtual void UpdateOHTTPKeyFor(const url::Origin& origin,
                                 const OHTTPKeyAndExpiration& key) = 0;

  virtual std::optional<KeyAndNonUniqueUserIdWithExpiration>
  GetKeyAndNonUniqueUserId() const = 0;
  virtual void UpdateKeyAndNonUniqueUserId(
      const KeyAndNonUniqueUserIdWithExpiration& key) = 0;
};

class KAnonymityServiceMemoryStorage : public KAnonymityServiceStorage {
 public:
  KAnonymityServiceMemoryStorage();
  ~KAnonymityServiceMemoryStorage() override;
  void WaitUntilReady(base::OnceCallback<void(InitStatus)> on_ready) override;

  std::optional<OHTTPKeyAndExpiration> GetOHTTPKeyFor(
      const url::Origin& origin) const override;
  void UpdateOHTTPKeyFor(const url::Origin& origin,
                         const OHTTPKeyAndExpiration& key) override;

  std::optional<KeyAndNonUniqueUserIdWithExpiration> GetKeyAndNonUniqueUserId()
      const override;
  void UpdateKeyAndNonUniqueUserId(
      const KeyAndNonUniqueUserIdWithExpiration& key) override;

 private:
  std::optional<KeyAndNonUniqueUserIdWithExpiration>
      key_and_non_unique_user_id_with_expiration_;
  base::flat_map<url::Origin, OHTTPKeyAndExpiration> ohttp_key_map_;
};

std::unique_ptr<KAnonymityServiceStorage> CreateKAnonymitySqlStorageForPath(
    base::FilePath db_storage_path);

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_STORAGE_H_
