// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/inmemory_file_index.h"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>

namespace file_manager {

InmemoryFileIndex::InmemoryFileIndex() = default;

InmemoryFileIndex::~InmemoryFileIndex() = default;

OpResults InmemoryFileIndex::UpdateFile(const std::vector<Term>& terms,
                                        const FileInfo& info) {
  if (terms.empty()) {
    return OpResults::kArgumentError;
  }
  return SetFileTerms(terms, info);
}

OpResults InmemoryFileIndex::RemoveFile(const GURL& url) {
  int64_t url_id = GetUrlId(url);
  if (url_id < 0) {
    return OpResults::kSuccess;
  }
  auto url_term_it = inverted_posting_lists_.find(url_id);
  if (url_term_it != inverted_posting_lists_.end()) {
    for (int64_t term_id : url_term_it->second) {
      RemoveFromPostingList(term_id, url_id);
    }
  }
  url_to_id_.erase(url);
  return OpResults::kSuccess;
}

OpResults InmemoryFileIndex::AugmentFile(const std::vector<Term>& terms,
                                         const FileInfo& info) {
  if (terms.empty()) {
    return OpResults::kSuccess;
  }

  int64_t url_id = GetOrCreateUrlId(info.file_url);
  DCHECK(url_id >= 0);
  PutFileInfo(url_id, info);

  std::set<int64_t> term_id_set = ConvertToTermIds(terms);
  AddFileTerms(term_id_set, url_id);
  return OpResults::kSuccess;
}

std::set<int64_t> InmemoryFileIndex::ConvertToTermIds(
    const std::vector<Term>& terms) {
  std::set<int64_t> term_ids;
  for (const Term& term : terms) {
    DCHECK(!term.field().empty());
    int64_t term_id = GetOrCreateTermId(term.text_bytes());
    term_ids.emplace(GetOrCreateAugmentedTermId(term.field(), term_id));
    term_ids.emplace(GetOrCreateAugmentedTermId("", term_id));
  }
  return term_ids;
}

OpResults InmemoryFileIndex::SetFileTerms(const std::vector<Term>& terms,
                                          const FileInfo& info) {
  DCHECK(!terms.empty());

  // Arrange terms by field and remove duplicates and convert to internal IDs.
  std::set<int64_t> term_id_set = ConvertToTermIds(terms);
  int64_t url_id = GetOrCreateUrlId(info.file_url);
  DCHECK(url_id >= 0);
  PutFileInfo(url_id, info);

  // If the given url_id already had some terms associated with it, remove terms
  // not specified in terms vector. Say, if url_id had terms {t1, t3, t8}
  // associated with it, and terms was {t1, t2}, we would compute {t3, t8} as
  // the difference between two collections and remove those.
  auto url_term_it = inverted_posting_lists_.find(url_id);
  if (url_term_it != inverted_posting_lists_.end()) {
    std::set<int64_t>& url_term_ids = url_term_it->second;
    std::vector<int64_t> to_remove_terms;
    std::set_difference(
        url_term_ids.begin(), url_term_ids.end(), term_id_set.begin(),
        term_id_set.end(),
        std::inserter(to_remove_terms, to_remove_terms.begin()));
    for (const int64_t term_id : to_remove_terms) {
      RemoveFromPostingList(term_id, url_id);
      RemoveFromTermList(url_id, term_id);
    }
  }
  AddFileTerms(term_id_set, url_id);
  return OpResults::kSuccess;
}

void InmemoryFileIndex::AddFileTerms(const std::set<int64_t>& term_ids,
                                     int64_t url_id) {
  for (const int64_t term_id : term_ids) {
    AddToPostingList(term_id, url_id);
    AddToTermList(url_id, term_id);
  }
}

void InmemoryFileIndex::AddToPostingList(int64_t term_id, int64_t url_id) {
  auto it = posting_lists_.find(term_id);
  if (it == posting_lists_.end()) {
    posting_lists_.emplace(std::piecewise_construct,
                           std::forward_as_tuple(term_id),
                           std::forward_as_tuple(std::set<int64_t>{url_id}));
  } else {
    it->second.insert(url_id);
  }
}

void InmemoryFileIndex::RemoveFromPostingList(int64_t term_id, int64_t url_id) {
  auto it = posting_lists_.find(term_id);
  if (it != posting_lists_.end()) {
    it->second.erase(url_id);
  }
}

void InmemoryFileIndex::AddToTermList(int64_t url_id, int64_t term_id) {
  auto it = inverted_posting_lists_.find(url_id);
  if (it == inverted_posting_lists_.end()) {
    inverted_posting_lists_.emplace(
        std::piecewise_construct, std::forward_as_tuple(url_id),
        std::forward_as_tuple(std::set<int64_t>{term_id}));
  } else {
    it->second.insert(term_id);
  }
}

void InmemoryFileIndex::RemoveFromTermList(int64_t url_id, int64_t term_id) {
  auto it = inverted_posting_lists_.find(url_id);
  if (it != inverted_posting_lists_.end()) {
    it->second.erase(term_id);
  }
}

int64_t InmemoryFileIndex::GetTermId(const std::string& term_bytes) const {
  auto it = term_map_.find(term_bytes);
  if (it != term_map_.end()) {
    return it->second;
  }
  return -1;
}

int64_t InmemoryFileIndex::GetOrCreateTermId(const std::string& term_bytes) {
  int64_t term_id = GetTermId(term_bytes);
  if (term_id >= 0) {
    return term_id;
  }
  const int64_t this_term_id = term_id_++;
  term_map_.emplace(std::make_pair(term_bytes, this_term_id));
  return this_term_id;
}

int64_t InmemoryFileIndex::GetAugmentedTermId(const std::string& field_name,
                                              int64_t term_id) const {
  std::tuple<std::string, int64_t> augmented_term{field_name, term_id};
  auto augmented_term_it = augmented_term_map_.find(augmented_term);
  if (augmented_term_it == augmented_term_map_.end()) {
    return -1;
  }
  return augmented_term_it->second;
}

int64_t InmemoryFileIndex::GetOrCreateAugmentedTermId(
    const std::string& field_name,
    int64_t term_id) {
  int64_t augmented_term_id = GetAugmentedTermId(field_name, term_id);
  if (augmented_term_id >= 0) {
    return augmented_term_id;
  }
  int64_t this_augmented_term_id = augmented_term_id_++;
  augmented_term_map_.emplace(std::make_pair(
      std::make_tuple(field_name, term_id), this_augmented_term_id));
  return this_augmented_term_id;
}

int64_t InmemoryFileIndex::GetUrlId(const GURL& url) {
  auto it = url_to_id_.find(url);
  return (it != url_to_id_.end()) ? it->second : -1;
}

int64_t InmemoryFileIndex::GetOrCreateUrlId(const GURL& url) {
  int64_t url_id = GetUrlId(url);
  if (url_id >= 0) {
    return url_id;
  }
  int64_t this_url_id = url_id_++;
  url_to_id_.emplace(std::make_pair(url, this_url_id));
  return this_url_id;
}

int64_t InmemoryFileIndex::PutFileInfo(int64_t url_id,
                                       const FileInfo& file_info) {
  DCHECK(url_id == GetUrlId(file_info.file_url));
  url_id_to_file_info_.emplace(std::make_pair(url_id, file_info));
  return url_id;
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
    int64_t term_id = GetTermId(term.text_bytes());
    if (term_id < 0) {
      return {};
    }
    int64_t augmented_term_id;
    if (term.field().empty()) {
      // Global search: this is the case of the user entering a query such as
      // "tax starred". We cannot tell if they mean "label:tax AND
      // label:starred" or "label:starred AND content:tax", etc. Unqualified
      // terms (those with empty field names) are searched in the global index.
      augmented_term_id = GetAugmentedTermId("", term_id);
    } else {
      augmented_term_id = GetAugmentedTermId(term.field(), term_id);
    }
    auto ith_term_match = posting_lists_.find(augmented_term_id);
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
