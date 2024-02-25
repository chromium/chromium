// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace arc {

class ArcDocumentsProviderRoot;
class ArcFileSystemOperationRunner;

// Container of ArcDocumentsProviderRoot instances.
//
// All member function must be called on the UI thread.
class ArcDocumentsProviderRootMap : public KeyedService {
 public:
  // This constructor should not be used. It is public for the sake of
  // compatibility with std::make_unique.
  explicit ArcDocumentsProviderRootMap(Profile* profile);
  ArcDocumentsProviderRootMap(const ArcDocumentsProviderRootMap&) = delete;
  ArcDocumentsProviderRootMap& operator=(const ArcDocumentsProviderRootMap&) =
      delete;

  ~ArcDocumentsProviderRootMap() override;

  // Returns an instance for the given browser context, or nullptr if ARC is not
  // allowed for the browser context.
  static ArcDocumentsProviderRootMap* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns an instance for the browser context associated with ARC, or nullptr
  // if ARC is not allowed.
  // TODO(nya): Remove this function when we support multi-user ARC. For now,
  // it is okay to call this function only from ash::FileSystemBackend and
  // its delegates.
  static ArcDocumentsProviderRootMap* GetForArcBrowserContext();

  // Looks up a root corresponding to |url|.
  // |path| is set to the remaining path part of |url|.
  // Returns nullptr if |url| is invalid or no corresponding root is registered.
  ArcDocumentsProviderRoot* ParseAndLookup(const storage::FileSystemURL& url,
                                           base::FilePath* path) const;

  // Looks up a root by an authority and a root document ID.
  ArcDocumentsProviderRoot* Lookup(const std::string& authority,
                                   const std::string& root_id) const;

  // Register a DocumentsProvider's Root to make the corresponding
  // ArcDocumentsProviderRoot instance available.
  void RegisterRoot(const std::string& authority,
                    const std::string& root_document_id,
                    const std::string& root_id,
                    bool read_only,
                    const std::vector<std::string>& mime_types);

  // Unregister a DocumentsProvider's Root.
  void UnregisterRoot(const std::string& authority, const std::string& root_id);

  // KeyedService overrides:
  void Shutdown() override;

 private:
  friend class ArcDocumentsProviderRootMapFactory;

  // |runner_| outlives |this| and ArcDocumentsProviderRoot instances in |map_|
  // as this service has explicit dependency on ArcFileSystemOperationRunner in
  // the BrowserContextKeyedServiceFactory dependency graph.
  const raw_ptr<ArcFileSystemOperationRunner> runner_;

  // Key is (authority, root_id).
  using Key = std::pair<std::string, std::string>;
  std::map<Key, std::unique_ptr<ArcDocumentsProviderRoot>> map_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_MAP_H_
