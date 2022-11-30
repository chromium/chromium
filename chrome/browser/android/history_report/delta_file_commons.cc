// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/delta_file_commons.h"

#include <stddef.h>

#include <iomanip>
#include <sstream>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "crypto/sha2.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using bookmarks::BookmarkModel;
using bookmarks::UrlAndTitle;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::GetCanonicalHostRegistryLength;

namespace {

const int kBookmarkScoreBonusMultiplier = 3;
const size_t kIdLengthLimit = 256;
const int kSHA256ByteSize = 32;
const size_t kUrlLengthLimit = 20 * 1024 * 1024; // 20M
const size_t kUrlLengthWidth = 8;

void StripTopLevelDomain(std::string* canonical_host) {
  size_t registry_length = GetCanonicalHostRegistryLength(
      *canonical_host, EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES);
  if (registry_length != 0 && registry_length != std::string::npos)
    canonical_host->erase(canonical_host->length() - (registry_length + 1));
}

void StripCommonSubDomains(std::string* host) {
  std::string www_prefix("www.");
  std::string ww2_prefix("ww2.");
  if (host->compare(0, www_prefix.size(), www_prefix) == 0) {
    host->erase(0, www_prefix.size());
  } else if (host->compare(0, ww2_prefix.size(), ww2_prefix) == 0) {
    host->erase(0, ww2_prefix.size());
  }
}

}  // namespace

namespace history_report {

DeltaFileEntryWithData::DeltaFileEntryWithData(DeltaFileEntry entry)
    : entry_(entry),
      data_set_(false),
      is_bookmark_(false) {}

DeltaFileEntryWithData::DeltaFileEntryWithData(
    const DeltaFileEntryWithData& other) = default;

DeltaFileEntryWithData::~DeltaFileEntryWithData() {}

int64_t DeltaFileEntryWithData::SeqNo() const {
  return entry_.seq_no();
}

std::string DeltaFileEntryWithData::Type() const {
  // If deletion entry has data then it's not a real deletion entry
  // but an update entry. Real deletion entry never has data.
  if (data_set_) return "add";
  return entry_.type();
}

// Generates a unique ID for a given URL.
// It must be shorter than or equal to |kIdLengthLimit| characters.
// If URL is shorter than or equal to |kIdLengthLimit| then ID is the URL
// itself. Otherwise it has a form of 3 concatenated parts:
//  1. Length of URL. Zero-padded integer to width |kUrlLengthWidth|,
//     because URLs are limited to 20M in Chrome.
//  2. SHA-256 of URL which takes 64 characters.
//  3. Prefix of URL of size |kIdLengthLimit| - 64 - |kUrlLengthWidth|.
std::string DeltaFileEntryWithData::UrlToId(const std::string& url) {
  if (url.size() > kUrlLengthLimit) {
    return "error: url too long";
  }

  if (IsValidId(url)) {
    return url;
  }

  std::stringstream id;

  // 1. Zero-padded URL length to width |kUrlLengthWidth|.
  id << std::setfill('0') << std::setw(kUrlLengthWidth) << url.size();

  // 2. SHA-256 of URL.
  uint8_t hash[kSHA256ByteSize];
  crypto::SHA256HashString(url, hash, sizeof(hash));
  id << base::HexEncode(hash, sizeof(hash));

  // 3. Prefix of URL to fill rest of the space.
  id << url.substr(0, kIdLengthLimit - 2 * kSHA256ByteSize - kUrlLengthWidth);

  return id.str();
}

// ID which identifies URL of this entry.
std::string DeltaFileEntryWithData::Id() const {
  return UrlToId(entry_.url());
}

std::string DeltaFileEntryWithData::Url() const {
  return entry_.url();
}

std::u16string DeltaFileEntryWithData::Title() const {
  if (!Valid())
    return u"";
  if (is_bookmark_ && !bookmark_title_.empty()) return bookmark_title_;
  if (data_.title().empty()) return base::UTF8ToUTF16(data_.url().host_piece());
  return data_.title();
}

int32_t DeltaFileEntryWithData::Score() const {
  if (!Valid()) return 0;
  int32_t score = data_.visit_count() + data_.typed_count();
  if (is_bookmark_) score = (score + 1) * kBookmarkScoreBonusMultiplier;
  return score;
}

std::string DeltaFileEntryWithData::IndexedUrl() const {
  if (!Valid()) return "";
  std::string indexed_url = data_.url().host();
  StripTopLevelDomain(&indexed_url);
  StripCommonSubDomains(&indexed_url);
  return indexed_url;
}

bool DeltaFileEntryWithData::Valid() const {
  return entry_.type() == "del" || is_bookmark_ ||
      (data_set_ && !data_.hidden());
}

void DeltaFileEntryWithData::SetData(const history::URLRow& data) {
  data_set_ = true;
  data_ = data;
}

void DeltaFileEntryWithData::MarkAsBookmark(const UrlAndTitle& bookmark) {
  is_bookmark_ = true;
  bookmark_title_ = bookmark.title;
}

// static
bool DeltaFileEntryWithData::IsValidId(const std::string& url) {
  return url.size() <= kIdLengthLimit;
}

}  // namespace history_report
