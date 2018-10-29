// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_query.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "components/download/public/common/download_item.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using download::DownloadDangerType;
using download::DownloadItem;

namespace {

// Templatized base::Value::GetAs*().
template <typename T> bool GetAs(const base::Value& in, T* out);
template<> bool GetAs(const base::Value& in, bool* out) {
  return in.GetAsBoolean(out);
}
template <>
bool GetAs(const base::Value& in, double* out) {
  return in.GetAsDouble(out);
}
template<> bool GetAs(const base::Value& in, std::string* out) {
  return in.GetAsString(out);
}
template<> bool GetAs(const base::Value& in, base::string16* out) {
  return in.GetAsString(out);
}
template<> bool GetAs(const base::Value& in, std::vector<base::string16>* out) {
  out->clear();
  const base::ListValue* list = NULL;
  if (!in.GetAsList(&list))
    return false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::string16 element;
    if (!list->GetString(i, &element)) {
      out->clear();
      return false;
    }
    out->push_back(element);
  }
  return true;
}

// The next several functions are helpers for making Callbacks that access
// DownloadItem fields.

int64_t GetStartTimeMsEpoch(const DownloadItem& item) {
  return (item.GetStartTime() - base::Time::UnixEpoch()).InMilliseconds();
}

int64_t GetEndTimeMsEpoch(const DownloadItem& item) {
  return (item.GetEndTime() - base::Time::UnixEpoch()).InMilliseconds();
}

std::string GetStartTime(const DownloadItem& item) {
  return base::TimeToISO8601(item.GetStartTime());
}

std::string GetEndTime(const DownloadItem& item) {
  return base::TimeToISO8601(item.GetEndTime());
}

bool GetDangerAccepted(const DownloadItem& item) {
  return (item.GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
}

bool GetExists(const DownloadItem& item) {
  return !item.GetFileExternallyRemoved();
}

base::string16 GetFilename(const DownloadItem& item) {
  // This filename will be compared with strings that could be passed in by the
  // user, who only sees LossyDisplayNames.
  return item.GetTargetFilePath().LossyDisplayName();
}

std::string GetFilenameUTF8(const DownloadItem& item) {
  return base::UTF16ToUTF8(GetFilename(item));
}

std::string GetOriginalUrl(const DownloadItem& item) {
  return item.GetOriginalUrl().spec();
}

std::string GetUrl(const DownloadItem& item) {
  return item.GetURL().spec();
}

DownloadItem::DownloadState GetState(const DownloadItem& item) {
  return item.GetState();
}

DownloadDangerType GetDangerType(const DownloadItem& item) {
  return item.GetDangerType();
}

double GetReceivedBytes(const DownloadItem& item) {
  return item.GetReceivedBytes();
}

double GetTotalBytes(const DownloadItem& item) {
  return item.GetTotalBytes();
}

std::string GetMimeType(const DownloadItem& item) {
  return item.GetMimeType();
}

bool IsPaused(const DownloadItem& item) {
  return item.IsPaused();
}

enum ComparisonType {LT, EQ, GT};

// Returns true if |item| matches the filter specified by |value|, |cmptype|,
// and |accessor|. |accessor| is conceptually a function that takes a
// DownloadItem and returns one of its fields, which is then compared to
// |value|.
template<typename ValueType>
bool FieldMatches(
    const ValueType& value,
    ComparisonType cmptype,
    const base::Callback<ValueType(const DownloadItem&)>& accessor,
    const DownloadItem& item) {
  switch (cmptype) {
    case LT: return accessor.Run(item) < value;
    case EQ: return accessor.Run(item) == value;
    case GT: return accessor.Run(item) > value;
  }
  NOTREACHED();
  return false;
}

// Helper for building a Callback to FieldMatches<>().
template <typename ValueType> DownloadQuery::FilterCallback BuildFilter(
    const base::Value& value, ComparisonType cmptype,
    ValueType (*accessor)(const DownloadItem&)) {
  ValueType cpp_value;
  if (!GetAs(value, &cpp_value)) return DownloadQuery::FilterCallback();
  return base::Bind(&FieldMatches<ValueType>, cpp_value, cmptype,
                    base::Bind(accessor));
}

// Returns true if |accessor.Run(item)| matches |pattern|.
bool FindRegex(
    RE2* pattern,
    const base::Callback<std::string(const DownloadItem&)>& accessor,
    const DownloadItem& item) {
  return RE2::PartialMatch(accessor.Run(item), *pattern);
}

// Helper for building a Callback to FindRegex().
DownloadQuery::FilterCallback BuildRegexFilter(
    const base::Value& regex_value,
    std::string (*accessor)(const DownloadItem&)) {
  std::string regex_str;
  if (!GetAs(regex_value, &regex_str)) return DownloadQuery::FilterCallback();
  std::unique_ptr<RE2> pattern(new RE2(regex_str));
  if (!pattern->ok()) return DownloadQuery::FilterCallback();
  return base::Bind(&FindRegex, base::Owned(pattern.release()),
                    base::Bind(accessor));
}

// Returns a ComparisonType to indicate whether a field in |left| is less than,
// greater than or equal to the same field in |right|.
template<typename ValueType>
ComparisonType Compare(
    const base::Callback<ValueType(const DownloadItem&)>& accessor,
    const DownloadItem& left, const DownloadItem& right) {
  ValueType left_value = accessor.Run(left);
  ValueType right_value = accessor.Run(right);
  if (left_value > right_value) return GT;
  if (left_value < right_value) return LT;
  DCHECK_EQ(left_value, right_value);
  return EQ;
}

}  // anonymous namespace

// static
bool DownloadQuery::MatchesQuery(const std::vector<base::string16>& query_terms,
                                 const DownloadItem& item) {
  if (query_terms.empty())
    return true;

  base::string16 original_url_raw(
      base::UTF8ToUTF16(item.GetOriginalUrl().spec()));
  base::string16 url_raw(base::UTF8ToUTF16(item.GetURL().spec()));
  // Try to also match query with above URLs formatted in user display friendly
  // way. This will unescape characters (including spaces) and trim all extra
  // data (like username and password) from raw url so that for example raw url
  // "http://some.server.org/example%20download/file.zip" will be matched with
  // search term "example download".
  base::string16 original_url_formatted(
      url_formatter::FormatUrl(item.GetOriginalUrl()));
  base::string16 url_formatted(url_formatter::FormatUrl(item.GetURL()));
  base::string16 path(item.GetTargetFilePath().LossyDisplayName());

  for (auto it = query_terms.begin(); it != query_terms.end(); ++it) {
    base::string16 term = base::i18n::ToLower(*it);
    if (!base::i18n::StringSearchIgnoringCaseAndAccents(
            term, original_url_raw, NULL, NULL) &&
        !base::i18n::StringSearchIgnoringCaseAndAccents(
            term, original_url_formatted, NULL, NULL) &&
        !base::i18n::StringSearchIgnoringCaseAndAccents(
            term, url_raw, NULL, NULL) &&
        !base::i18n::StringSearchIgnoringCaseAndAccents(
            term, url_formatted, NULL, NULL) &&
        !base::i18n::StringSearchIgnoringCaseAndAccents(
            term, path, NULL, NULL)) {
      return false;
    }
  }
  return true;
}

DownloadQuery::DownloadQuery() : limit_(std::numeric_limits<uint32_t>::max()) {}
DownloadQuery::~DownloadQuery() {}

// AddFilter() pushes a new FilterCallback to filters_. Most FilterCallbacks are
// Callbacks to FieldMatches<>(). Search() iterates over given DownloadItems,
// discarding items for which any filter returns false. A DownloadQuery may have
// zero or more FilterCallbacks.

bool DownloadQuery::AddFilter(const DownloadQuery::FilterCallback& value) {
  if (value.is_null()) return false;
  filters_.push_back(value);
  return true;
}

void DownloadQuery::AddFilter(DownloadItem::DownloadState state) {
  AddFilter(base::Bind(&FieldMatches<DownloadItem::DownloadState>, state, EQ,
      base::Bind(&GetState)));
}

void DownloadQuery::AddFilter(DownloadDangerType danger) {
  AddFilter(base::Bind(&FieldMatches<DownloadDangerType>, danger, EQ,
      base::Bind(&GetDangerType)));
}

bool DownloadQuery::AddFilter(DownloadQuery::FilterType type,
                              const base::Value& value) {
  switch (type) {
    case FILTER_BYTES_RECEIVED:
      return AddFilter(BuildFilter<double>(value, EQ, &GetReceivedBytes));
    case FILTER_DANGER_ACCEPTED:
      return AddFilter(BuildFilter<bool>(value, EQ, &GetDangerAccepted));
    case FILTER_EXISTS:
      return AddFilter(BuildFilter<bool>(value, EQ, &GetExists));
    case FILTER_FILENAME:
      return AddFilter(BuildFilter<base::string16>(value, EQ, &GetFilename));
    case FILTER_FILENAME_REGEX:
      return AddFilter(BuildRegexFilter(value, &GetFilenameUTF8));
    case FILTER_MIME:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetMimeType));
    case FILTER_PAUSED:
      return AddFilter(BuildFilter<bool>(value, EQ, &IsPaused));
    case FILTER_QUERY: {
      std::vector<base::string16> query_terms;
      return GetAs(value, &query_terms) &&
             (query_terms.empty() ||
              AddFilter(base::Bind(&MatchesQuery, query_terms)));
    }
    case FILTER_ENDED_AFTER:
      return AddFilter(BuildFilter<std::string>(value, GT, &GetEndTime));
    case FILTER_ENDED_BEFORE:
      return AddFilter(BuildFilter<std::string>(value, LT, &GetEndTime));
    case FILTER_END_TIME:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetEndTime));
    case FILTER_STARTED_AFTER:
      return AddFilter(BuildFilter<std::string>(value, GT, &GetStartTime));
    case FILTER_STARTED_BEFORE:
      return AddFilter(BuildFilter<std::string>(value, LT, &GetStartTime));
    case FILTER_START_TIME:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetStartTime));
    case FILTER_TOTAL_BYTES:
      return AddFilter(BuildFilter<double>(value, EQ, &GetTotalBytes));
    case FILTER_TOTAL_BYTES_GREATER:
      return AddFilter(BuildFilter<double>(value, GT, &GetTotalBytes));
    case FILTER_TOTAL_BYTES_LESS:
      return AddFilter(BuildFilter<double>(value, LT, &GetTotalBytes));
    case FILTER_ORIGINAL_URL:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetOriginalUrl));
    case FILTER_ORIGINAL_URL_REGEX:
      return AddFilter(BuildRegexFilter(value, &GetOriginalUrl));
    case FILTER_URL:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetUrl));
    case FILTER_URL_REGEX:
      return AddFilter(BuildRegexFilter(value, &GetUrl));
  }
  return false;
}

bool DownloadQuery::Matches(const DownloadItem& item) const {
  for (auto filter = filters_.begin(); filter != filters_.end(); ++filter) {
    if (!filter->Run(item))
      return false;
  }
  return true;
}

// AddSorter() creates a Sorter and pushes it onto sorters_. A Sorter is a
// direction and a Callback to Compare<>(). After filtering, Search() makes a
// DownloadComparator functor from the sorters_ and passes the
// DownloadComparator to std::partial_sort. std::partial_sort calls the
// DownloadComparator with different pairs of DownloadItems.  DownloadComparator
// iterates over the sorters until a callback returns ComparisonType LT or GT.
// DownloadComparator returns true or false depending on that ComparisonType and
// the sorter's direction in order to indicate to std::partial_sort whether the
// left item is after or before the right item. If all sorters return EQ, then
// DownloadComparator compares GetId. A DownloadQuery may have zero or more
// Sorters, but there is one DownloadComparator per call to Search().

struct DownloadQuery::Sorter {
  typedef base::Callback<ComparisonType(
      const DownloadItem&, const DownloadItem&)> SortType;

  template<typename ValueType>
  static Sorter Build(DownloadQuery::SortDirection adirection,
                         ValueType (*accessor)(const DownloadItem&)) {
    return Sorter(adirection, base::Bind(&Compare<ValueType>,
        base::Bind(accessor)));
  }

  Sorter(DownloadQuery::SortDirection adirection,
            const SortType& asorter)
    : direction(adirection),
      sorter(asorter) {
  }
  ~Sorter() {}

  DownloadQuery::SortDirection direction;
  SortType sorter;
};

class DownloadQuery::DownloadComparator {
 public:
  explicit DownloadComparator(const DownloadQuery::SorterVector& terms)
    : terms_(terms) {
  }

  // Returns true if |left| sorts before |right|.
  bool operator() (const DownloadItem* left, const DownloadItem* right);

 private:
  const DownloadQuery::SorterVector& terms_;

  // std::sort requires this class to be copyable.
};

bool DownloadQuery::DownloadComparator::operator() (
    const DownloadItem* left, const DownloadItem* right) {
  for (auto term = terms_.begin(); term != terms_.end(); ++term) {
    switch (term->sorter.Run(*left, *right)) {
      case LT: return term->direction == DownloadQuery::ASCENDING;
      case GT: return term->direction == DownloadQuery::DESCENDING;
      case EQ: break;  // break the switch but not the loop
    }
  }
  CHECK_NE(left->GetId(), right->GetId());
  return left->GetId() < right->GetId();
}

void DownloadQuery::AddSorter(DownloadQuery::SortType type,
                              DownloadQuery::SortDirection direction) {
  switch (type) {
    case SORT_END_TIME:
      sorters_.push_back(Sorter::Build<int64_t>(direction, &GetEndTimeMsEpoch));
      break;
    case SORT_START_TIME:
      sorters_.push_back(
          Sorter::Build<int64_t>(direction, &GetStartTimeMsEpoch));
      break;
    case SORT_ORIGINAL_URL:
      sorters_.push_back(
          Sorter::Build<std::string>(direction, &GetOriginalUrl));
      break;
    case SORT_URL:
      sorters_.push_back(Sorter::Build<std::string>(direction, &GetUrl));
      break;
    case SORT_FILENAME:
      sorters_.push_back(
          Sorter::Build<base::string16>(direction, &GetFilename));
      break;
    case SORT_DANGER:
      sorters_.push_back(Sorter::Build<DownloadDangerType>(
          direction, &GetDangerType));
      break;
    case SORT_DANGER_ACCEPTED:
      sorters_.push_back(Sorter::Build<bool>(direction, &GetDangerAccepted));
      break;
    case SORT_EXISTS:
      sorters_.push_back(Sorter::Build<bool>(direction, &GetExists));
      break;
    case SORT_STATE:
      sorters_.push_back(Sorter::Build<DownloadItem::DownloadState>(
          direction, &GetState));
      break;
    case SORT_PAUSED:
      sorters_.push_back(Sorter::Build<bool>(direction, &IsPaused));
      break;
    case SORT_MIME:
      sorters_.push_back(Sorter::Build<std::string>(direction, &GetMimeType));
      break;
    case SORT_BYTES_RECEIVED:
      sorters_.push_back(Sorter::Build<double>(direction, &GetReceivedBytes));
      break;
    case SORT_TOTAL_BYTES:
      sorters_.push_back(Sorter::Build<double>(direction, &GetTotalBytes));
      break;
  }
}

void DownloadQuery::FinishSearch(DownloadQuery::DownloadVector* results) const {
 if (!sorters_.empty()) {
    std::partial_sort(results->begin(),
                      results->begin() + std::min(limit_, results->size()),
                      results->end(),
                      DownloadComparator(sorters_));
  }

  if (results->size() > limit_)
    results->resize(limit_);
}
