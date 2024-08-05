// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/model/data_type_store.h"

class Profile;

namespace webapk {

// An abstract database factory mockable for testing.
class AbstractWebApkDatabaseFactory {
 public:
  virtual ~AbstractWebApkDatabaseFactory() = default;
  virtual syncer::OnceDataTypeStoreFactory GetStoreFactory() = 0;
};

// Creates a DataTypeStoreFactory per profile.
class WebApkDatabaseFactory : public AbstractWebApkDatabaseFactory {
 public:
  explicit WebApkDatabaseFactory(Profile* profile);
  WebApkDatabaseFactory(const WebApkDatabaseFactory&) = delete;
  WebApkDatabaseFactory& operator=(const WebApkDatabaseFactory&) = delete;
  ~WebApkDatabaseFactory() override;

  // AbstractWebApkDatabaseFactory implementation.
  syncer::OnceDataTypeStoreFactory GetStoreFactory() override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_DATABASE_FACTORY_H_
