// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_WEBAPK_DATABASE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_WEBAPK_DATABASE_FACTORY_H_

#include <memory>

#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "components/sync/model/model_type_store.h"

namespace webapk {

class WebApkProto;

// WebApkDatabaseFactory subclass with direct read/write access to the registry.
class FakeWebApkDatabaseFactory : public AbstractWebApkDatabaseFactory {
 public:
  FakeWebApkDatabaseFactory();
  FakeWebApkDatabaseFactory(const FakeWebApkDatabaseFactory&) = delete;
  FakeWebApkDatabaseFactory& operator=(const FakeWebApkDatabaseFactory&) =
      delete;
  ~FakeWebApkDatabaseFactory() override;

  syncer::ModelTypeStore* GetStore();

  syncer::OnceModelTypeStoreFactory GetStoreFactory() override;

  Registry ReadRegistry();

  void WriteProtos(const std::vector<const WebApkProto*>& protos);
  void WriteRegistry(const Registry& registry);

 private:
  std::unique_ptr<syncer::ModelTypeStore> store_;
};
}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_WEBAPK_DATABASE_FACTORY_H_
