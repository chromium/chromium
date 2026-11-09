// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_cache_manager.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "third_party/zstd/src/lib/zstd.h"
#include "url/gurl.h"

namespace {

// Retention for the cache. Set at 30 minutes, since the cache is only useful
// when Help me read is used again on a recently used page.
constexpr auto kCacheRetention = base::Minutes(30);

// zstd level 1 is the fastest standard level. Page content can be several MB
// (e.g. PDFs) and is compressed on the UI thread, so favor speed over ratio.
constexpr int kZstdCompressionLevel = 1;

std::vector<uint8_t> CompressPageContent(std::u16string_view content) {
  const base::span<const uint8_t> input = base::as_byte_span(content);
  std::vector<uint8_t> buffer(ZSTD_compressBound(input.size()));
  const size_t compressed_size =
      ZSTD_compress(buffer.data(), buffer.size(), input.data(), input.size(),
                    kZstdCompressionLevel);
  CHECK(!ZSTD_isError(compressed_size));
  // Copy into an exact-sized vector to release the unused bound capacity.
  const auto compressed = base::span(buffer).first(compressed_size);
  return std::vector<uint8_t>(compressed.begin(), compressed.end());
}

std::u16string DecompressPageContent(base::span<const uint8_t> compressed) {
  // A default-constructed `MahiData` has no compressed content.
  if (compressed.empty()) {
    return std::u16string();
  }
  const uint64_t size =
      ZSTD_getFrameContentSize(compressed.data(), compressed.size());
  CHECK_NE(size, ZSTD_CONTENTSIZE_ERROR);
  CHECK_NE(size, ZSTD_CONTENTSIZE_UNKNOWN);
  CHECK_EQ(size % sizeof(char16_t), 0u);
  std::u16string content(size / sizeof(char16_t), u'\0');
  const size_t decompressed_size = ZSTD_decompress(
      content.data(), size, compressed.data(), compressed.size());
  CHECK_EQ(decompressed_size, size);
  return content;
}

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
      compressed_page_content(CompressPageContent(page_content)),
      favicon_image(favicon_image),
      summary(summary),
      previous_qa(previous_qa),
      creation_time(base::Time::Now()) {}

MahiCacheManager::MahiData::MahiData(const MahiData&) = default;

MahiCacheManager::MahiData& MahiCacheManager::MahiData::operator=(
    const MahiCacheManager::MahiData&) = default;

MahiCacheManager::MahiData::~MahiData() = default;

std::u16string MahiCacheManager::MahiData::GetPageContent() const {
  return DecompressPageContent(compressed_page_content);
}

MahiCacheManager::MahiCacheManager() : page_cache_(kMaxCacheEntries) {}

MahiCacheManager::~MahiCacheManager() = default;

void MahiCacheManager::AddCacheForUrl(const std::string& url,
                                      const MahiData& data) {
  const auto gurl = GURL(url).GetWithoutRef();
  if (gurl.SchemeIsHTTPOrHTTPS()) {
    RemoveExpiredEntries();
    page_cache_.Put(gurl, data);
  }
}

void MahiCacheManager::TryToUpdateSummaryForUrl(const std::string& url,
                                                const std::u16string& summary) {
  const auto gurl = GURL(url).GetWithoutRef();
  if (!gurl.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  RemoveExpiredEntries();
  auto it = page_cache_.Get(gurl);
  if (it != page_cache_.end()) {
    it->second.summary = summary;
  }
}

std::u16string MahiCacheManager::GetPageContentForUrl(
    const std::string& url) const {
  RemoveExpiredEntries();
  auto it = page_cache_.Get(GURL(url).GetWithoutRef());
  return it != page_cache_.end() ? it->second.GetPageContent()
                                 : std::u16string();
}

std::optional<std::u16string> MahiCacheManager::GetSummaryForUrl(
    const std::string& url) const {
  RemoveExpiredEntries();
  auto it = page_cache_.Get(GURL(url).GetWithoutRef());
  return it != page_cache_.end() ? it->second.summary : std::nullopt;
}

std::vector<MahiCacheManager::MahiQA> MahiCacheManager::GetQAForUrl(
    const std::string& url) const {
  RemoveExpiredEntries();
  auto it = page_cache_.Get(GURL(url).GetWithoutRef());
  return it != page_cache_.end() ? it->second.previous_qa
                                 : std::vector<MahiQA>({});
}

void MahiCacheManager::ClearCache() {
  page_cache_.Clear();
}

void MahiCacheManager::DeleteCacheForUrl(const std::string& url) {
  auto it = page_cache_.Peek(GURL(url).GetWithoutRef());
  if (it != page_cache_.end()) {
    page_cache_.Erase(it);
  }
}

int MahiCacheManager::size() const {
  RemoveExpiredEntries();
  return page_cache_.size();
}

void MahiCacheManager::RemoveExpiredEntries() const {
  const base::Time now = base::Time::Now();
  for (auto iter = page_cache_.begin(); iter != page_cache_.end();
       /* no increment */) {
    if ((now - iter->second.creation_time) >= kCacheRetention) {
      iter = page_cache_.Erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace ash
