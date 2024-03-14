// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_cache_manager.h"

#include <string>

#include "mahi_cache_manager.h"
#include "url/gurl.h"

namespace ash {

MahiCacheManager::MahiData::MahiData() = default;

MahiCacheManager::MahiData::MahiData(
    const std::string& url,
    const std::u16string& title,
    const std::optional<gfx::ImageSkia>& favicon_image,
    const std::u16string& page_content,
    const std::u16string& summary,
    const std::vector<MahiQA>& previous_qa)
    : url(url),
      title(title),
      favicon_image(favicon_image),
      page_content(page_content),
      summary(summary),
      previous_qa(previous_qa) {}

MahiCacheManager::MahiData::MahiData(const MahiData&) = default;

MahiCacheManager::MahiData& MahiCacheManager::MahiData::operator=(
    const MahiCacheManager::MahiData&) = default;

MahiCacheManager::MahiData::~MahiData() = default;

MahiCacheManager::MahiCacheManager() = default;

MahiCacheManager::~MahiCacheManager() = default;

void MahiCacheManager::AddCacheForUrl(const std::string& url,
                                      const MahiData& data) {
  auto gurl = GURL(url).GetWithoutRef();
  page_cache_[gurl] = data;
}

std::optional<std::u16string> MahiCacheManager::GetSummaryForUrl(
    const std::string& url) const {
  auto gurl = GURL(url).GetWithoutRef();
  return page_cache_.contains(gurl)
             ? std::make_optional(page_cache_.at(gurl).summary)
             : std::nullopt;
}

std::vector<MahiCacheManager::MahiQA> MahiCacheManager::GetQAForUrl(
    const std::string& url) const {
  auto gurl = GURL(url).GetWithoutRef();
  return page_cache_.contains(gurl) ? page_cache_.at(gurl).previous_qa
                                    : std::vector<MahiQA>({});
}

void MahiCacheManager::ClearCache() {
  page_cache_.clear();
}

}  // namespace ash
