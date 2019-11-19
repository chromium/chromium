// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/documents_provider_root_manager.h"

#include <string.h>

#include <algorithm>
#include <iterator>
#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_features.h"
#include "components/arc/mojom/bitmap.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace file_manager {

namespace {

// Some authorities should be excluded from the list of DocumentsProvider roots
// since equivalent contents are already exposed in Files app.
constexpr const char* kAuthoritiesToExclude[] = {
    // Android internal "external storage" is already exposed as "Play Files".
    "com.android.externalstorage.documents",
    // Android's Downloads directory is already shared with Chrome OS's
    // Downloads.
    "com.android.providers.downloads.documents",
    // Documents from media providers are already exposed as media views.
    "com.android.providers.media.documents",
    // Files app already has its own Google Drive integration.
    "com.google.android.apps.docs.storage",
};

bool IsAuthorityToExclude(const std::string& authority) {
  for (const char* authority_to_exclude : kAuthoritiesToExclude) {
    if (base::EqualsCaseInsensitiveASCII(authority, authority_to_exclude))
      return true;
  }
  return false;
}

GURL EncodeIconAsUrl(const SkBitmap& bitmap) {
  // Root icons are resized to 32px*32px in the ARC container. We use the given
  // bitmaps without resizing in Chrome side.
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(output.data()),
                        output.size()),
      &encoded);
  return GURL("data:image/png;base64," + encoded);
}

// A wrapper class for SkBitmap to compare its pixels.
class BitmapWrapper {
 public:
  explicit BitmapWrapper(const SkBitmap* bitmap) : bitmap_(bitmap) {}
  bool operator<(const BitmapWrapper& other) const {
    const size_t size1 = bitmap_->computeByteSize();
    const size_t size2 = other.bitmap_->computeByteSize();
    if (size1 == 0 && size2 == 0)
      return false;
    if (size1 != size2)
      return size1 < size2;
    return memcmp(bitmap_->getAddr32(0, 0), other.bitmap_->getAddr32(0, 0),
                  size1) < 0;
  }

 private:
  const SkBitmap* const bitmap_;
  DISALLOW_COPY_AND_ASSIGN(BitmapWrapper);
};

}  // namespace

DocumentsProviderRootManager::DocumentsProviderRootManager(
    Profile* profile,
    arc::ArcFileSystemOperationRunner* runner)
    : profile_(profile), runner_(runner) {}

DocumentsProviderRootManager::~DocumentsProviderRootManager() {
  arc::ArcFileSystemBridge* bridge =
      arc::ArcFileSystemBridge::GetForBrowserContext(profile_);
  if (bridge)
    bridge->RemoveObserver(this);
}

void DocumentsProviderRootManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observer_list_.AddObserver(observer);
}

void DocumentsProviderRootManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observer_list_.RemoveObserver(observer);
}

void DocumentsProviderRootManager::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(
          arc::kEnableDocumentsProviderInFilesAppFeature)) {
    return;
  }
  if (enabled == is_enabled_)
    return;

  is_enabled_ = enabled;
  arc::ArcFileSystemBridge* bridge =
      arc::ArcFileSystemBridge::GetForBrowserContext(profile_);
  if (enabled) {
    if (bridge)
      bridge->AddObserver(this);
    RequestGetRoots();
  } else {
    if (bridge)
      bridge->RemoveObserver(this);
    ClearRoots();
  }
}

void DocumentsProviderRootManager::OnRootsChanged() {
  // Instead of process delta updates, we get full roots list every time roots
  // list is updated in ARC container, and overwrite the current volume list
  // based on the returned roots list.
  // Note that observer's callbacks are called only for roots which are
  // added/removed/modified.
  RequestGetRoots();
}

DocumentsProviderRootManager::RootInfo::RootInfo() = default;

DocumentsProviderRootManager::RootInfo::RootInfo(const RootInfo& that) =
    default;

DocumentsProviderRootManager::RootInfo::RootInfo(RootInfo&& that) noexcept =
    default;

DocumentsProviderRootManager::RootInfo::~RootInfo() = default;

DocumentsProviderRootManager::RootInfo& DocumentsProviderRootManager::RootInfo::
operator=(const RootInfo& that) = default;

DocumentsProviderRootManager::RootInfo& DocumentsProviderRootManager::RootInfo::
operator=(RootInfo&& that) noexcept = default;

bool DocumentsProviderRootManager::RootInfo::operator<(
    const RootInfo& rhs) const {
  BitmapWrapper wrapped_icon(&icon);
  BitmapWrapper wrapped_rhs_icon(&rhs.icon);
  return std::tie(authority, root_id, document_id, title, summary,
                  wrapped_icon) < std::tie(rhs.authority, rhs.root_id,
                                           rhs.document_id, rhs.title,
                                           rhs.summary, wrapped_rhs_icon);
}

void DocumentsProviderRootManager::RequestGetRoots() {
  runner_->GetRoots(base::BindOnce(&DocumentsProviderRootManager::OnGetRoots,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void DocumentsProviderRootManager::OnGetRoots(
    base::Optional<std::vector<arc::mojom::RootPtr>> maybe_roots) {
  if (!maybe_roots.has_value())
    return;

  std::vector<RootInfo> roots_info;
  for (const auto& root : maybe_roots.value()) {
    if (IsAuthorityToExclude(root->authority))
      continue;

    RootInfo root_info;
    root_info.authority = root->authority;
    root_info.root_id = root->root_id;
    root_info.document_id = root->document_id;
    root_info.title = root->title;
    if (root->summary.has_value())
      root_info.summary = root->summary.value();
    if (root->icon.has_value())
      root_info.icon = root->icon.value();
    root_info.supports_create = root->supports_create;
    if (root->mime_types.has_value())
      root_info.mime_types = root->mime_types.value();

    roots_info.emplace_back(std::move(root_info));
  }
  std::sort(roots_info.begin(), roots_info.end());
  UpdateRoots(std::move(roots_info));
}

void DocumentsProviderRootManager::UpdateRoots(
    std::vector<RootInfo> new_roots) {
  // |roots_to_remove| should have roots which were in the previous list but
  // do not exist in the new list.
  std::vector<RootInfo> roots_to_remove;
  std::set_difference(current_roots_.begin(), current_roots_.end(),
                      new_roots.begin(), new_roots.end(),
                      std::inserter(roots_to_remove, roots_to_remove.begin()));
  // |roots_to_add| should have roots which were not in the previous list but
  // exist in the new list.
  std::vector<RootInfo> roots_to_add;
  std::set_difference(new_roots.begin(), new_roots.end(),
                      current_roots_.begin(), current_roots_.end(),
                      std::inserter(roots_to_add, roots_to_add.begin()));
  for (const auto& info : roots_to_remove) {
    NotifyRootRemoved(info);
  }
  for (const auto& info : roots_to_add) {
    NotifyRootAdded(info);
  }
  current_roots_.swap(new_roots);
}

void DocumentsProviderRootManager::ClearRoots() {
  UpdateRoots({});
}

void DocumentsProviderRootManager::NotifyRootAdded(const RootInfo& info) {
  for (auto& observer : observer_list_) {
    observer.OnDocumentsProviderRootAdded(
        info.authority, info.root_id, info.document_id, info.title,
        info.summary,
        !info.icon.empty() ? EncodeIconAsUrl(info.icon) : GURL::EmptyGURL(),
        !info.supports_create, info.mime_types);
  }
}

void DocumentsProviderRootManager::NotifyRootRemoved(const RootInfo& info) {
  for (auto& observer : observer_list_) {
    observer.OnDocumentsProviderRootRemoved(info.authority, info.root_id,
                                            info.document_id);
  }
}

}  // namespace file_manager
