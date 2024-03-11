// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"

#include <algorithm>

namespace file_manager {

FileIndexService::FileIndexService(Profile* profile) {
  DCHECK(profile);
}

FileIndexService::~FileIndexService() = default;

void FileIndexService::UpdateFile(const std::vector<Term>& terms,
                                  const FileInfo& info) {
  if (terms.empty()) {
    RemoveFile(info.file_url);
  } else {
    AddFile(terms, info);
  }
}

bool FileIndexService::RemoveFile(const GURL& url) {
  int64_t url_id = GetUrlId(url);
  if (url_id < 0) {
    return false;
  }
  auto term_it = term_lists_.find(url_id);
  if (term_it == term_lists_.end()) {
    url_to_id_.erase(url);
    return true;
  }
  for (int64_t term_id : term_it->second) {
    auto posting_it = posting_lists_.find(term_id);
    DCHECK(posting_it != posting_lists_.end());
    posting_it->second.erase(url_id);
  }
  term_lists_.erase(term_it);
  url_to_id_.erase(url);
  return true;
}

void FileIndexService::AddFile(const std::vector<Term>& terms,
                               const FileInfo& info) {
  DCHECK(!terms.empty());

  // Eliminate duplciates and convert to internal IDs.
  std::set<int64_t> term_ids;
  for (const Term& term : terms) {
    term_ids.insert(GetTermId(term.text_bytes(), true));
    term_ids.insert(GetTermId(term.value(), true));
  }
  int64_t url_id = GetOrCreateUrlId(info);
  DCHECK(url_id >= 0);

  auto it = term_lists_.find(url_id);
  if (it != term_lists_.end()) {
    std::set<int64_t>& file_terms = it->second;
    std::vector<int64_t> to_remove_terms;
    std::set_difference(
        file_terms.begin(), file_terms.end(), term_ids.begin(), term_ids.end(),
        std::inserter(to_remove_terms, to_remove_terms.begin()));
    for (const int64_t term_id : to_remove_terms) {
      RemoveFromPostingList(term_id, url_id);
      RemoveFromTermList(url_id, term_id);
    }
  }

  for (const int64_t term_id : term_ids) {
    AddToPostingList(term_id, url_id);
    AddToTermList(url_id, term_id);
  }
}

void FileIndexService::AddToPostingList(int64_t term_id, int64_t url_id) {
  auto it = posting_lists_.find(term_id);
  if (it == posting_lists_.end()) {
    std::set<int64_t> fresh_set{url_id};
    posting_lists_.emplace(std::make_pair(term_id, fresh_set));
  } else {
    it->second.insert(url_id);
  }
}

void FileIndexService::RemoveFromPostingList(int64_t term_id, int64_t url_id) {
  auto it = posting_lists_.find(term_id);
  if (it != posting_lists_.end()) {
    it->second.erase(url_id);
  }
}

void FileIndexService::AddToTermList(int64_t url_id, int64_t term_id) {
  auto it = term_lists_.find(url_id);
  if (it == term_lists_.end()) {
    std::set<int64_t> fresh_set{term_id};
    term_lists_.emplace(std::make_pair(url_id, fresh_set));
  } else {
    it->second.insert(term_id);
  }
}

void FileIndexService::RemoveFromTermList(int64_t url_id, int64_t term_id) {
  auto it = term_lists_.find(url_id);
  if (it != term_lists_.end()) {
    it->second.erase(term_id);
  }
}

int64_t FileIndexService::GetTermId(const std::string& term_bytes,
                                    bool create) {
  auto it = term_map_.find(term_bytes);
  if (it != term_map_.end()) {
    return it->second;
  }
  if (!create) {
    return -1;
  }
  const int64_t this_term_id = term_id_++;
  term_map_.emplace(std::make_pair(term_bytes, this_term_id));
  return this_term_id;
}

int64_t FileIndexService::GetUrlId(const GURL& url) {
  auto it = url_to_id_.find(url);
  return (it != url_to_id_.end()) ? it->second : -1;
}

int64_t FileIndexService::GetOrCreateUrlId(const FileInfo& info) {
  int64_t url_id = GetUrlId(info.file_url);
  if (url_id >= 0) {
    return url_id;
  }
  int64_t this_url_id = url_id_++;
  url_to_id_.emplace(std::make_pair(info.file_url, this_url_id));
  url_id_to_file_info_.emplace(std::make_pair(this_url_id, info));
  return this_url_id;
}

// Searches the index for file info matching the specified query.
std::vector<FileInfo> FileIndexService::Search(const Query& query) {
  const std::vector<Term>& terms = query.terms();
  if (terms.empty()) {
    // Technically, an empty query matches every file, but we treat this
    // as empty match.
    return {};
  }
  std::set<int64_t> matched_url_ids;
  bool first = true;
  for (const Term& term : terms) {
    int64_t term_id = GetTermId(term.value(), false);
    if (term_id < 0) {
      return {};
    }
    auto ith_term_match = posting_lists_.find(term_id);
    if (ith_term_match == posting_lists_.end()) {
      return {};
    }
    if (first) {
      matched_url_ids = ith_term_match->second;
      first = false;
    } else {
      std::set<int64_t> intersection;
      std::set_intersection(matched_url_ids.begin(), matched_url_ids.end(),
                            ith_term_match->second.begin(),
                            ith_term_match->second.end(),
                            std::inserter(intersection, intersection.begin()));
      matched_url_ids = intersection;
    }
    if (matched_url_ids.empty()) {
      break;
    }
  }
  if (matched_url_ids.empty()) {
    return {};
  }
  std::vector<FileInfo> info_list;
  for (const int64_t url_id : matched_url_ids) {
    auto file_info_it = url_id_to_file_info_.find(url_id);
    DCHECK(file_info_it != url_id_to_file_info_.end());
    info_list.emplace_back(file_info_it->second);
  }
  return info_list;
}

}  // namespace file_manager
