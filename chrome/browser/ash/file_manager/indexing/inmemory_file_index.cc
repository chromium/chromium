// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/inmemory_file_index.h"

#include <algorithm>
#include <cstdint>

namespace file_manager {

InmemoryFileIndex::InmemoryFileIndex() = default;

InmemoryFileIndex::~InmemoryFileIndex() = default;

void InmemoryFileIndex::UpdateFile(const std::vector<Term>& terms,
                                   const FileInfo& info) {
  if (terms.empty()) {
    RemoveFile(info.file_url);
  } else {
    SetFileTerms(terms, info);
  }
}

bool InmemoryFileIndex::RemoveFile(const GURL& url) {
  int64_t url_id = GetUrlId(url);
  if (url_id < 0) {
    return false;
  }
  for (auto& [field_name, term_lists] : term_namespace_) {
    auto term_it = term_lists.find(url_id);
    if (term_it == term_lists.end()) {
      continue;
    }
    for (int64_t term_id : term_it->second) {
      auto field_it = posting_namespace_.find(field_name);
      if (field_it == posting_namespace_.end()) {
        continue;
      }
      PostingLists& posting_lists = field_it->second;
      auto posting_it = posting_lists.find(term_id);
      DCHECK(posting_it != posting_lists.end());
      posting_it->second.erase(url_id);
    }
    term_lists.erase(term_it);
  }
  url_to_id_.erase(url);
  return true;
}

void InmemoryFileIndex::AugmentFile(const std::vector<Term>& terms,
                                    const FileInfo& info) {
  if (terms.empty()) {
    return;
  }

  int64_t url_id = GetOrCreateUrlId(info);
  DCHECK(url_id >= 0);

  // TODO(majewski): Factor out making term_ids_by_field.
  std::map<std::string, std::set<int64_t>> term_ids_by_field;
  for (const Term& term : terms) {
    auto it = term_ids_by_field.find(term.field());
    int64_t term_id = GetTermId(term.text_bytes(), true);
    if (it == term_ids_by_field.end()) {
      std::set<int64_t> term_set{term_id};
      term_ids_by_field.emplace(std::make_pair(term.field(), term_set));
    } else {
      it->second.insert(term_id);
    }
  }
  for (const auto& by_field_it : term_ids_by_field) {
    AddFileTerms(by_field_it.first, by_field_it.second, url_id);
  }
}

void InmemoryFileIndex::SetFileTerms(const std::vector<Term>& terms,
                                     const FileInfo& info) {
  DCHECK(!terms.empty());

  // TODO(majewski): Factor out making term_ids_by_field.
  // Arrange terms by field and remove duplicates and convert to internal IDs.
  std::map<std::string, std::set<int64_t>> term_ids_by_field;
  for (const Term& term : terms) {
    auto it = term_ids_by_field.find(term.field());
    int64_t term_id = GetTermId(term.text_bytes(), true);
    if (it == term_ids_by_field.end()) {
      std::set<int64_t> term_set{term_id};
      term_ids_by_field.emplace(std::make_pair(term.field(), term_set));
    } else {
      it->second.insert(term_id);
    }
  }

  int64_t url_id = GetOrCreateUrlId(info);
  DCHECK(url_id >= 0);

  // If the given url_id already had some terms associated with it, remove terms
  // not specified in terms vector. Say, if url_id had terms {t1, t3, t8}
  // associated with it, and terms was {t1, t2}, we would compute {t3, t8} as
  // the difference between two collections and remove those.
  for (const auto& by_field_it : term_ids_by_field) {
    const std::string& field_name = by_field_it.first;
    const std::set<int64_t>& term_ids = by_field_it.second;
    auto field_it = term_namespace_.find(field_name);
    if (field_it == term_namespace_.end()) {
      continue;
    }
    TermLists& term_lists = field_it->second;
    auto it = term_lists.find(url_id);
    if (it == term_lists.end()) {
      continue;
    }
    std::set<int64_t>& file_terms = it->second;
    std::vector<int64_t> to_remove_terms;
    std::set_difference(
        file_terms.begin(), file_terms.end(), term_ids.begin(), term_ids.end(),
        std::inserter(to_remove_terms, to_remove_terms.begin()));
    for (const int64_t term_id : to_remove_terms) {
      RemoveFromPostingList(field_name, term_id, url_id);
      RemoveFromTermList(field_name, url_id, term_id);
    }
  }
  for (const auto& by_field_it : term_ids_by_field) {
    AddFileTerms(by_field_it.first, by_field_it.second, url_id);
  }
}

void InmemoryFileIndex::AddFileTerms(const std::string& field_name,
                                     const std::set<int64_t>& term_ids,
                                     int64_t url_id) {
  for (const int64_t term_id : term_ids) {
    AddToPostingList(field_name, term_id, url_id);
    AddToTermList(field_name, url_id, term_id);
  }
}

void InmemoryFileIndex::AddToPostingList(const std::string& field_name,
                                         int64_t term_id,
                                         int64_t url_id) {
  auto namespace_it = posting_namespace_.find(field_name);
  if (namespace_it == posting_namespace_.end()) {
    bool is_inserted;
    std::tie(namespace_it, is_inserted) = posting_namespace_.emplace(
        std::make_pair(field_name, std::map<int64_t, std::set<int64_t>>()));
    DCHECK(is_inserted);
  }
  PostingLists& posting_lists = namespace_it->second;
  auto it = posting_lists.find(term_id);
  if (it == posting_lists.end()) {
    std::set<int64_t> fresh_set{url_id};
    posting_lists.emplace(std::make_pair(term_id, fresh_set));
  } else {
    it->second.insert(url_id);
  }
}

void InmemoryFileIndex::RemoveFromPostingList(const std::string& field_name,
                                              int64_t term_id,
                                              int64_t url_id) {
  auto namespace_it = posting_namespace_.find(field_name);
  if (namespace_it == posting_namespace_.end()) {
    return;
  }
  PostingLists& posting_lists = namespace_it->second;
  auto it = posting_lists.find(term_id);
  if (it != posting_lists.end()) {
    it->second.erase(url_id);
  }
}

void InmemoryFileIndex::AddToTermList(const std::string& field_name,
                                      int64_t url_id,
                                      int64_t term_id) {
  auto field_it = term_namespace_.find(field_name);
  if (field_it == term_namespace_.end()) {
    bool is_inserted;
    TermLists term_lists;
    std::tie(field_it, is_inserted) =
        term_namespace_.emplace(std::make_pair(field_name, term_lists));
    DCHECK(is_inserted);
  }
  TermLists& term_lists = field_it->second;
  auto it = term_lists.find(url_id);
  if (it == term_lists.end()) {
    std::set<int64_t> fresh_set{term_id};
    term_lists.emplace(std::make_pair(url_id, fresh_set));
  } else {
    it->second.insert(term_id);
  }
}

void InmemoryFileIndex::RemoveFromTermList(const std::string& field_name,
                                           int64_t url_id,
                                           int64_t term_id) {
  auto field_it = term_namespace_.find(field_name);
  if (field_it == term_namespace_.end()) {
    return;
  }
  TermLists& term_lists = field_it->second;
  auto it = term_lists.find(url_id);
  if (it != term_lists.end()) {
    it->second.erase(term_id);
  }
}

int64_t InmemoryFileIndex::GetTermId(const std::string& term_bytes,
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

int64_t InmemoryFileIndex::GetUrlId(const GURL& url) {
  auto it = url_to_id_.find(url);
  return (it != url_to_id_.end()) ? it->second : -1;
}

int64_t InmemoryFileIndex::GetOrCreateUrlId(const FileInfo& info) {
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
std::vector<FileInfo> InmemoryFileIndex::Search(const Query& query) {
  const std::vector<Term>& terms = query.terms();
  if (terms.empty()) {
    // Technically, an empty query matches every file, but we treat this
    // as empty match.
    return {};
  }
  std::set<int64_t> matched_url_ids;
  bool first = true;
  for (const Term& term : terms) {
    int64_t term_id = GetTermId(term.text_bytes(), false);
    if (term_id < 0) {
      return {};
    }
    auto field_it = posting_namespace_.find(term.field());
    if (field_it == posting_namespace_.end()) {
      return {};
    }
    PostingLists& posting_lists = field_it->second;
    auto ith_term_match = posting_lists.find(term_id);
    if (ith_term_match == posting_lists.end()) {
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
