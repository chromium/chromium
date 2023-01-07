// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_manager.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/task/task_runner.h"
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

IconManager::IconManager() = default;

IconManager::~IconManager() = default;

gfx::Image* IconManager::LookupIconFromFilepath(const base::FilePath& file_path,
                                                IconLoader::IconSize size,
                                                float scale) {
  // Since loading the icon is synchronous on Chrome OS (and doesn't require
  // disk access), if it hasn't already been loaded, load immediately.
#if BUILDFLAG(IS_CHROMEOS)
  gfx::Image* image = DoLookupIconFromFilepath(file_path, size, scale);
  if (image)
    return image;

  IconLoader::LoadIcon(
      file_path, size, scale,
      base::BindOnce(&IconManager::OnIconLoaded, weak_factory_.GetWeakPtr(),
                     base::DoNothing(), file_path, size, scale));
#endif
  return DoLookupIconFromFilepath(file_path, size, scale);
}

base::CancelableTaskTracker::TaskId IconManager::LoadIcon(
    const base::FilePath& file_path,
    IconLoader::IconSize size,
    float scale,
    IconRequestCallback callback,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);
  IconRequestCallback callback_runner = base::BindOnce(
      &RunCallbackIfNotCanceled, is_canceled, std::move(callback));

  IconLoader::LoadIcon(
      file_path, size, scale,
      base::BindOnce(&IconManager::OnIconLoaded, weak_factory_.GetWeakPtr(),
                     std::move(callback_runner), file_path, size, scale));

  return id;
}

gfx::Image* IconManager::DoLookupIconFromFilepath(
    const base::FilePath& file_path,
    IconLoader::IconSize size,
    float scale) {
  auto group_it = group_cache_.find(file_path);
  if (group_it == group_cache_.end())
    return nullptr;

  CacheKey key(group_it->second, size, scale);
  auto icon_it = icon_cache_.find(key);
  if (icon_it == icon_cache_.end())
    return nullptr;

  return &icon_it->second;
}

void IconManager::OnIconLoaded(IconRequestCallback callback,
                               base::FilePath file_path,
                               IconLoader::IconSize size,
                               float scale,
                               gfx::Image result,
                               const IconLoader::IconGroup& group) {
  // Cache the bitmap. Watch out: |result| may be null, which indicates a
  // failure. We assume that if we have an entry in |icon_cache_| it must not be
  // null.
  CacheKey key(group, size, scale);
  std::move(callback).Run(result);
  if (!result.IsEmpty())
    icon_cache_[key] = std::move(result);
  else
    icon_cache_.erase(key);

  group_cache_[file_path] = group;
}

IconManager::CacheKey::CacheKey(const IconLoader::IconGroup& group,
                                IconLoader::IconSize size,
                                float scale)
    : group(group), size(size), scale(scale) {}

bool IconManager::CacheKey::operator<(const CacheKey& other) const {
  return std::tie(group, size, scale) <
         std::tie(other.group, other.size, other.scale);
}
