// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/task_runner.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace {

void RunCallbackIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    IconManager::IconRequestCallback callback,
    gfx::Image image) {
  if (is_canceled.Run())
    return;
  std::move(callback).Run(std::move(image));
}

}  // namespace

IconManager::IconManager() {}

IconManager::~IconManager() {
}

gfx::Image* IconManager::LookupIconFromFilepath(const base::FilePath& file_path,
                                                IconLoader::IconSize size) {
  auto group_it = group_cache_.find(file_path);
  if (group_it == group_cache_.end())
    return nullptr;

  CacheKey key(group_it->second, size);
  auto icon_it = icon_cache_.find(key);
  if (icon_it == icon_cache_.end())
    return nullptr;

  return &icon_it->second;
}

base::CancelableTaskTracker::TaskId IconManager::LoadIcon(
    const base::FilePath& file_path,
    IconLoader::IconSize size,
    IconRequestCallback callback,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);
  IconRequestCallback callback_runner = base::BindOnce(
      &RunCallbackIfNotCanceled, is_canceled, std::move(callback));

  IconLoader* loader = IconLoader::Create(
      file_path, size,
      base::BindOnce(&IconManager::OnIconLoaded, weak_factory_.GetWeakPtr(),
                     std::move(callback_runner), file_path, size));
  loader->Start();

  return id;
}

void IconManager::OnIconLoaded(IconRequestCallback callback,
                               base::FilePath file_path,
                               IconLoader::IconSize size,
                               gfx::Image result,
                               const IconLoader::IconGroup& group) {
  // Cache the bitmap. Watch out: |result| may be null, which indicates a
  // failure. We assume that if we have an entry in |icon_cache_| it must not be
  // null.
  CacheKey key(group, size);
  std::move(callback).Run(result);
  if (!result.IsEmpty())
    icon_cache_[key] = std::move(result);
  else
    icon_cache_.erase(key);

  group_cache_[file_path] = group;
}

IconManager::CacheKey::CacheKey(const IconLoader::IconGroup& group,
                                IconLoader::IconSize size)
    : group(group), size(size) {}

bool IconManager::CacheKey::operator<(const CacheKey &other) const {
  return std::tie(group, size) < std::tie(other.group, other.size);
}
