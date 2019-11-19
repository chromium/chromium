// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_mounter.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"

using content::BrowserThread;

namespace arc {
namespace {

// Singleton factory for ArcFileSystemMounter.
class ArcFileSystemMounterFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemMounter,
          ArcFileSystemMounterFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcFileSystemMounterFactory";

  static ArcFileSystemMounterFactory* GetInstance() {
    return base::Singleton<ArcFileSystemMounterFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemMounterFactory>;
  ArcFileSystemMounterFactory() = default;
  ~ArcFileSystemMounterFactory() override = default;
};

}  // namespace

// static
ArcFileSystemMounter* ArcFileSystemMounter::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcFileSystemMounterFactory::GetForBrowserContext(context);
}

ArcFileSystemMounter::ArcFileSystemMounter(content::BrowserContext* context,
                                           ArcBridgeService* bridge_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  mount_points->RegisterFileSystem(
      kContentFileSystemMountPointName, storage::kFileSystemTypeArcContent,
      storage::FileSystemMountOption(),
      base::FilePath(kContentFileSystemMountPointPath));
  mount_points->RegisterFileSystem(
      kDocumentsProviderMountPointName,
      storage::kFileSystemTypeArcDocumentsProvider,
      storage::FileSystemMountOption(),
      base::FilePath(kDocumentsProviderMountPointPath));
}

ArcFileSystemMounter::~ArcFileSystemMounter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  mount_points->RevokeFileSystem(kContentFileSystemMountPointName);
  mount_points->RevokeFileSystem(kDocumentsProviderMountPointPath);
}

}  // namespace arc
