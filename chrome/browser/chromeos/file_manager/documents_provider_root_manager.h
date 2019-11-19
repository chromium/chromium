// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DOCUMENTS_PROVIDER_ROOT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DOCUMENTS_PROVIDER_ROOT_MANAGER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_bridge.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

class Profile;

namespace arc {

class ArcFileSystemOperationRunner;

}  // namespace arc

namespace file_manager {

// Keeps track of available DocumentsProvider roots.
//
// This interacts with ARC container via Mojo API, and keeps track of currently
// available roots.
// In addition, This class filters roots so that File Manager doesn't have
// duplicated volumes. (e.g. Google Drive root from ARC container should be
// filtered out since Files app has its own Google Drive integration.)
// This notifies its observers about roots which are added, deleted, or
// modified.
// When metadata of a root is modified, both OnDocumentsProviderRootRemoved()
// and OnDocumentsProviderRootAdded() will be called for observers in this
// order.
// Note that this class's instance will not interact with ARC container until
// SetEnabled(true) is called. This is not enable by default.
class DocumentsProviderRootManager : public arc::ArcFileSystemBridge::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new available root is found. When an existing root is
    // modified, both RootRemoved() and RootAdded() will be called in this
    // order.
    virtual void OnDocumentsProviderRootAdded(
        const std::string& authority,
        const std::string& root_id,
        const std::string& document_id,
        const std::string& title,
        const std::string& summary,
        const GURL& icon_url,
        bool read_only,
        const std::vector<std::string>& mime_types) = 0;

    // Called when an existing root is not available anymore. When an existing
    // root is modified, both RootRemoved() and RootAdded() will be called in
    // this order.
    virtual void OnDocumentsProviderRootRemoved(
        const std::string& authority,
        const std::string& root_id,
        const std::string& document_id) = 0;
  };
  DocumentsProviderRootManager(Profile* profile,
                               arc::ArcFileSystemOperationRunner* runner);
  ~DocumentsProviderRootManager() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Allows DocumentsProviderRootManager retrieve DocumentsProvider roots from
  // ARC container.
  void SetEnabled(bool enabled);

  // ArcFileSystemBridge::Observer overrides:
  void OnRootsChanged() override;

 private:
  struct RootInfo {
    std::string authority;
    std::string root_id;
    std::string document_id;
    std::string title;
    std::string summary;
    SkBitmap icon;
    bool supports_create;
    std::vector<std::string> mime_types;

    RootInfo();
    RootInfo(const RootInfo& that);
    RootInfo(RootInfo&& that) noexcept;
    ~RootInfo();
    RootInfo& operator=(const RootInfo& that);
    RootInfo& operator=(RootInfo&& that) noexcept;
    bool operator<(const RootInfo& rhs) const;
  };

  // Requests ArcFileSystemOperationRunner to retrieve available roots from ARC
  // container.
  void RequestGetRoots();

  // Called when retrieving available roots from ARC container is done.
  void OnGetRoots(base::Optional<std::vector<arc::mojom::RootPtr>> maybe_roots);

  // Updates this class's internal list of available roots.
  void UpdateRoots(std::vector<RootInfo> roots);

  // Clears this class's internal list of available roots.
  void ClearRoots();

  // Notifies observers that a new available root is found.
  void NotifyRootAdded(const RootInfo& root_info);

  // Notifies observers that an existing root is removed.
  void NotifyRootRemoved(const RootInfo& root_info);

  Profile* const profile_;
  arc::ArcFileSystemOperationRunner* const runner_;
  bool is_enabled_ = false;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::vector<RootInfo> current_roots_;

  base::WeakPtrFactory<DocumentsProviderRootManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DocumentsProviderRootManager);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_DOCUMENTS_PROVIDER_ROOT_MANAGER_H_
