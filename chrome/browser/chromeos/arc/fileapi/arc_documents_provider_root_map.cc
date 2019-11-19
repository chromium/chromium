// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map_factory.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace arc {

namespace {

struct DocumentsProviderSpec {
  const char* authority;
  const char* root_document_id;
  const char* root_id;
  bool read_only;
};

// List of documents providers for media views.
constexpr DocumentsProviderSpec kDocumentsProviderWhitelist[] = {
    {"com.android.providers.media.documents", "images_root", "images_root",
     true},
    {"com.android.providers.media.documents", "videos_root", "videos_root",
     true},
    {"com.android.providers.media.documents", "audio_root", "audio_root", true},
};

}  // namespace

// static
ArcDocumentsProviderRootMap* ArcDocumentsProviderRootMap::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDocumentsProviderRootMapFactory::GetForBrowserContext(context);
}

// static
ArcDocumentsProviderRootMap*
ArcDocumentsProviderRootMap::GetForArcBrowserContext() {
  return GetForBrowserContext(ArcServiceManager::Get()->browser_context());
}

ArcDocumentsProviderRootMap::ArcDocumentsProviderRootMap(Profile* profile)
    : runner_(ArcFileSystemOperationRunner::GetForBrowserContext(profile)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // ArcDocumentsProviderRootMap is created only for the profile with ARC
  // in ArcDocumentsProviderRootMapFactory.
  DCHECK(runner_);

  for (const auto& spec : kDocumentsProviderWhitelist) {
    RegisterRoot(spec.authority, spec.root_document_id, spec.root_id,
                 spec.read_only, {});
  }
}

ArcDocumentsProviderRootMap::~ArcDocumentsProviderRootMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(map_.empty());
}

ArcDocumentsProviderRoot* ArcDocumentsProviderRootMap::ParseAndLookup(
    const storage::FileSystemURL& url,
    base::FilePath* path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string authority;
  std::string root_document_id;
  base::FilePath tmp_path;
  if (!ParseDocumentsProviderUrl(url, &authority, &root_document_id, &tmp_path))
    return nullptr;

  ArcDocumentsProviderRoot* root = Lookup(authority, root_document_id);
  if (!root)
    return nullptr;

  *path = tmp_path;
  return root;
}

ArcDocumentsProviderRoot* ArcDocumentsProviderRootMap::Lookup(
    const std::string& authority,
    const std::string& root_document_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = map_.find(Key(authority, root_document_id));
  if (iter == map_.end())
    return nullptr;
  return iter->second.get();
}

void ArcDocumentsProviderRootMap::RegisterRoot(
    const std::string& authority,
    const std::string& root_document_id,
    const std::string& root_id,
    bool read_only,
    const std::vector<std::string>& mime_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Key key(authority, root_document_id);
  if (map_.find(key) != map_.end()) {
    VLOG(1) << "Trying to register (" << authority << ", " << root_document_id
            << ") which is already regisered.";
    return;
  }
  map_.emplace(key, std::make_unique<ArcDocumentsProviderRoot>(
                        runner_, authority, root_document_id, root_id,
                        read_only, mime_types));
}

void ArcDocumentsProviderRootMap::UnregisterRoot(
    const std::string& authority,
    const std::string& root_document_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!map_.erase(Key(authority, root_document_id))) {
    VLOG(1) << "Trying to unregister (" << authority << ", " << root_document_id
            << ") which is not registered.";
  }
}

void ArcDocumentsProviderRootMap::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // ArcDocumentsProviderRoot has a reference to another KeyedService
  // (ArcFileSystemOperationRunner), so we need to destruct them on shutdown.
  map_.clear();
}

}  // namespace arc
