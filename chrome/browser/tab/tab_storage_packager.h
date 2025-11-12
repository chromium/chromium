// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_

#include <memory>
#include <string>

#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/children.pb.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"

namespace tabs {
class TabInterface;
class TabCollection;
class StoragePackage;
class StorageIdMapping;

// This class is used to package tab data for use in the background thread.
class TabStoragePackager {
 public:
  TabStoragePackager();
  virtual ~TabStoragePackager();

  TabStoragePackager(const TabStoragePackager&) = delete;
  TabStoragePackager& operator=(const TabStoragePackager&) = delete;

  // Packages the tab's data for later use.
  virtual std::unique_ptr<StoragePackage> Package(const TabInterface* tab) = 0;

  // Packages an arbitrary tab collection's state for later use. Conceptually
  // just this collection is represented by the package, not parents or
  // children's data. However the identity and order of children should be
  // captured in this package.
  std::unique_ptr<StoragePackage> Package(const TabCollection* collection,
                                          StorageIdMapping& mapping);

  // Packages only the children of a collection for storage.
  std::unique_ptr<Payload> PackageChildren(const TabCollection* collection,
                                           StorageIdMapping& mapping);

  // Packages only the payload of a collection for storage.
  std::unique_ptr<Payload> PackagePayload(const TabCollection* collection,
                                          StorageIdMapping& mapping);

  // Whether the `collection`'s root node is off-the-record.
  virtual bool IsOffTheRecord(const TabCollection* collection) const = 0;

  // Returns a unique window tag for the `collection`'s root node. On Android,
  // the window tag may be shared between an off-the-record and a regular
  // collection.
  virtual std::string GetWindowTag(const TabCollection* collection) const = 0;

 protected:
  virtual std::unique_ptr<Payload> PackageTabStripCollectionData(
      const TabStripCollection* collection,
      StorageIdMapping& mapping) = 0;

  const TabCollection* GetRootCollection(const TabCollection* collection) const;

 private:
  std::unique_ptr<Payload> PackageTabGroupTabCollectionData(
      const TabGroupTabCollection* collection,
      StorageIdMapping& mapping);
  std::unique_ptr<Payload> PackageSplitTabCollectionData(
      const SplitTabCollection* collection,
      StorageIdMapping& mapping);
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_
