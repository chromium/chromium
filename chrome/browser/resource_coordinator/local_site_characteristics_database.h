// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATABASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATABASE_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data.pb.h"
#include "url/origin.h"

namespace resource_coordinator {

// Interface for a local site characteristic database.
class LocalSiteCharacteristicsDatabase {
 public:
  // Callback to call once the initialization from the database has completed,
  // |site_characteristic_proto| should be equal to base::nullopt if the
  // initialization has failed.
  using ReadSiteCharacteristicsFromDBCallback = base::OnceCallback<void(
      base::Optional<SiteDataProto> site_characteristic_proto)>;
  using GetDatabaseSizeCallback =
      base::OnceCallback<void(base::Optional<int64_t> num_rows,
                              base::Optional<int64_t> on_disk_size_kb)>;

  LocalSiteCharacteristicsDatabase() = default;
  virtual ~LocalSiteCharacteristicsDatabase() {}

  // Checks the if there's an entry with the key |site_origin| and if so use it
  // to initialize |site_characteristic_proto|. Calls |callback| to indicate
  // whether or not the initialization has been successful.
  virtual void ReadSiteCharacteristicsFromDB(
      const url::Origin& origin,
      ReadSiteCharacteristicsFromDBCallback callback) = 0;

  // Store an entry in the database, create it if it doesn't exist and update it
  // if it does.
  virtual void WriteSiteCharacteristicsIntoDB(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto) = 0;

  // Removes some entries from the database.
  virtual void RemoveSiteCharacteristicsFromDB(
      const std::vector<url::Origin>& site_origins) = 0;

  // Clear the database, removes every entries that it contains.
  virtual void ClearDatabase() = 0;

  // Retrieve the size of the database.
  virtual void GetDatabaseSize(GetDatabaseSizeCallback callback) = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATABASE_H_
