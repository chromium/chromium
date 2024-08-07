// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_cache_manager.h"

#include <memory>
#include <string>

#include "url/gurl.h"

namespace {

// Interval at which we check for cache deletion.
constexpr auto kClearingInterval = base::Minutes(30);

// Retention for the cache. Set at 7 days.
constexpr auto kCacheRetention = base::Days(7);

}  // namespace

namespace ash {

MahiCacheManager::MahiData::MahiData() : creation_time(base::Time::Now()) {}

MahiCacheManager::MahiData::MahiData(
    const std::string& url,
    const std::u16string& title,
    const std::u16string& page_content,
    const std::optional<gfx::ImageSkia>& favicon_image,
    const std::optional<std::u16string>& summary,
    const std::vector<MahiQA>& previous_qa)
    : url(url),
      title(title),
      page_content(page_content),
      favicon_image(favicon_image),
      summary(summary),
      previous_qa(previous_qa),
      creation_time(base::Time::Now()) {}

MahiCacheManager::MahiData::MahiData(const MahiData&) = default;

MahiCacheManager::MahiData& MahiCacheManager::MahiData::operator=(
    const MahiCacheManager::MahiData&) = default;

MahiCacheManager::MahiData::~MahiData() = default;

MahiCacheManager::MahiCacheManager()
    : periodic_timer_(std::make_unique<base::RepeatingTimer>()) {
  periodic_timer_->Start(FROM_HERE, kClearingInterval, this,
                         &MahiCacheManager::OnTimerFired);
}

MahiCacheManager::~MahiCacheManager() = default;

void MahiCacheManager::AddCacheForUrl(const std::string& url,
                                      const MahiData& data) {
  const auto gurl = GURL(url).GetWithoutRef();
  if (gurl.SchemeIsHTTPOrHTTPS()) {
    page_cache_[gurl] = data;
  }
}

void MahiCacheManager::TryToUpdateSummaryForUrl(const std::string& url,
                                                const std::u16string& summary) {
  const auto gurl = GURL(url).GetWithoutRef();
  if (!page_cache_.contains(gurl) || !gurl.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  page_cache_[gurl].summary = summary;
}

std::u16string MahiCacheManager::GetPageContentForUrl(
    const std::string& url) const {
  const auto gurl = GURL(url).GetWithoutRef();
  return page_cache_.contains(gurl) ? page_cache_.at(gurl).page_content
                                    : std::u16string();
}

std::optional<std::u16string> MahiCacheManager::GetSummaryForUrl(
    const std::string& url) const {
  const auto gurl = GURL(url).GetWithoutRef();
  return page_cache_.contains(gurl) ? page_cache_.at(gurl).summary
                                    : std::nullopt;
}

std::vector<MahiCacheManager::MahiQA> MahiCacheManager::GetQAForUrl(
    const std::string& url) const {
  const auto gurl = GURL(url).GetWithoutRef();
  return page_cache_.contains(gurl) ? page_cache_.at(gurl).previous_qa
                                    : std::vector<MahiQA>({});
}

void MahiCacheManager::ClearCache() {
  page_cache_.clear();
}

void MahiCacheManager::DeleteCacheForUrl(const std::string& url) {
  const auto it = page_cache_.find(GURL(url).GetWithoutRef());
  if (it != page_cache_.end()) {
    page_cache_.erase(it);
  }
}

void MahiCacheManager::OnTimerFired() {
  for (auto iter = page_cache_.begin(); iter != page_cache_.end();
       /* no increment */) {
    if ((base::Time::Now() - iter->second.creation_time) >= kCacheRetention) {
      iter = page_cache_.erase(iter);
    } else {
      iter++;
    }
  }
}

}  // namespace ash
