// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace file_manager {

class FileIndexService;

// A factory of FileIndexService. Initialized after a user login. Typical use:
//
// FileIndexService* service = FileIndexServiceFactory::GetForBrowserContext(
//     profile);
// auto matched_files = service->Query(query);
class FileIndexServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FileIndexService* GetForBrowserContext(
      content::BrowserContext* context);
  static FileIndexServiceFactory* GetInstance();

  FileIndexServiceFactory(const FileIndexServiceFactory&) = delete;
  FileIndexServiceFactory& operator=(const FileIndexServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<FileIndexServiceFactory>;

  FileIndexServiceFactory();
  ~FileIndexServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_INDEX_INDEX_SERVICE_FACTORY_H_
