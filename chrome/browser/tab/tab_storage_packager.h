// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_

#include <memory>
#include <string>

namespace tabs {
class TabInterface;
class TabCollection;
class StoragePackage;

// This class is used to package tab data for use in the background thread.
class TabStoragePackager {
 public:
  TabStoragePackager() = default;
  virtual ~TabStoragePackager() = default;

  TabStoragePackager(const TabStoragePackager&) = delete;
  TabStoragePackager& operator=(const TabStoragePackager&) = delete;

  // Packages the tab's data for later use. After packaging a tab, its data
  // is available via the #ReleasePackage() method.
  virtual void Package(const TabInterface* tab) = 0;

  // Packages an aribtrtary tab collection's state for later use. Conceptually
  // just this collection is represented by the package, not parents or
  // children's data. However the identity and order of children shold be
  // captured in this package. After packaging, its is available via the
  // #ReleasePackage() method.
  virtual void Package(const TabCollection* collection) = 0;

  // Allows the unique ownership of the underlying StoragePackage to be
  // transferred out of the packager. After this call, the stored package will
  // be null.
  virtual std::unique_ptr<StoragePackage> ReleasePackage() = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_
