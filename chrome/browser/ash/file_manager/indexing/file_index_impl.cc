// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_impl.h"

#include "base/time/time.h"

namespace file_manager {

FileIndexImpl::FileIndexImpl(std::unique_ptr<IndexStorage> storage)
    : storage_(std::move(storage)) {}
FileIndexImpl::~FileIndexImpl() = default;

bool FileIndexImpl::Init() {
  return storage_->Init();
}

OpResults FileIndexImpl::PutFileInfo(const FileInfo& file_info) {
  return storage_->PutFileInfo(file_info) == -1 ? OpResults::kGenericError
                                                : OpResults::kSuccess;
}

OpResults FileIndexImpl::UpdateFile(const std::vector<Term>& terms,
                                    const GURL& url) {
  if (terms.empty()) {
    return OpResults::kArgumentError;
  }
  return SetFileTerms(terms, url);
}

OpResults FileIndexImpl::RemoveFile(const GURL& url) {
  int64_t url_id = storage_->GetUrlId(url);
  if (url_id < 0) {
    return OpResults::kSuccess;
  }
  const std::set<int64_t>& url_term_ids =
      storage_->GetAugmentedTermIdsForUrl(url_id);
  for (int64_t term_id : url_term_ids) {
    storage_->DeleteFromPostingList(term_id, url_id);
  }
  storage_->DeleteFileInfo(url_id);
  storage_->DeleteUrl(url);
  return OpResults::kSuccess;
}

OpResults FileIndexImpl::RemoveTerms(const std::vector<Term>& terms,
                                     const GURL& url) {
  int64_t url_id = storage_->GetUrlId(url);
  if (url_id < 0) {
    return OpResults::kSuccess;
  }
  std::set<int64_t> augmented_term_ids;
  for (const Term& t : terms) {
    int64_t id_with_field = storage_->GetAugmentedTermId(t);
    if (id_with_field != -1) {
      augmented_term_ids.emplace(id_with_field);
    }
    int64_t global_id = storage_->GetAugmentedTermId(Term("", t.text()));
    if (global_id != -1) {
      augmented_term_ids.emplace(global_id);
    }
  }
  for (int64_t augmented_term_id : augmented_term_ids) {
    storage_->DeleteFromPostingList(augmented_term_id, url_id);
  }
  return OpResults::kSuccess;
}

OpResults FileIndexImpl::AugmentFile(const std::vector<Term>& terms,
                                     const GURL& url) {
  if (terms.empty()) {
    return OpResults::kSuccess;
  }

  int64_t url_id = storage_->GetUrlId(url);
  if (url_id == -1) {
    return OpResults::kFileMissing;
  }

  std::set<int64_t> term_id_set = ConvertToAugmentedTermIds(terms);
  storage_->AddAugmentedTermIdsForUrl(term_id_set, url_id);
  return OpResults::kSuccess;
}

// Searches the index for file info matching the specified query.
SearchResults FileIndexImpl::Search(const Query& query) {
  const std::vector<Term>& terms = query.terms();
  SearchResults results;
  if (terms.empty()) {
    // Technically, an empty query matches every file, but we treat this
    // as empty match.
    return results;
  }
  std::set<int64_t> matched_url_ids;
  bool first = true;
  for (const Term& term : terms) {
    int64_t augmented_term_id = storage_->GetAugmentedTermId(term);
    if (augmented_term_id == -1) {
      return results;
    }
    const std::set<int64_t> url_ids =
        storage_->GetUrlIdsForAugmentedTermId(augmented_term_id);
    if (url_ids.empty()) {
      return results;
    }
    if (first) {
      matched_url_ids = url_ids;
      first = false;
    } else {
      std::set<int64_t> intersection;
      std::set_intersection(matched_url_ids.begin(), matched_url_ids.end(),
                            url_ids.begin(), url_ids.end(),
                            std::inserter(intersection, intersection.begin()));
      matched_url_ids = intersection;
    }
    if (matched_url_ids.empty()) {
      break;
    }
  }
  if (matched_url_ids.empty()) {
    return results;
  }
  for (const int64_t url_id : matched_url_ids) {
    FileInfo file_info(GURL(""), 0u, base::Time());
    int64_t found_url_id = storage_->GetFileInfo(url_id, &file_info);
    DCHECK(found_url_id == url_id);
    // TODO(b:327535200): Add true score.
    results.matches.emplace_back(Match(1, file_info));
  }
  // TODO(b:327535200): Correctly compute total_matches.
  results.total_matches = results.matches.size();
  return results;
}

std::set<int64_t> FileIndexImpl::ConvertToAugmentedTermIds(
    const std::vector<Term>& terms) {
  std::set<int64_t> term_ids;
  for (const Term& term : terms) {
    DCHECK(!term.field().empty());
    term_ids.emplace(storage_->GetOrCreateAugmentedTermId(term));
    term_ids.emplace(
        storage_->GetOrCreateAugmentedTermId(Term("", term.text())));
  }
  return term_ids;
}

OpResults FileIndexImpl::SetFileTerms(const std::vector<Term>& terms,
                                      const GURL& url) {
  DCHECK(!terms.empty());

  // Arrange terms by field and remove duplicates and convert to internal IDs.
  std::set<int64_t> term_id_set = ConvertToAugmentedTermIds(terms);
  int64_t url_id = storage_->GetUrlId(url);
  if (url_id == -1) {
    return OpResults::kFileMissing;
  }

  // If the given url_id already had some terms associated with it, remove terms
  // not specified in terms vector. Say, if url_id had terms {t1, t3, t8}
  // associated with it, and terms was {t1, t2}, we would compute {t3, t8} as
  // the difference between two collections and remove those.
  std::set<int64_t> url_term_ids = storage_->GetAugmentedTermIdsForUrl(url_id);
  if (!url_term_ids.empty()) {
    std::set<int64_t> to_remove_terms;
    std::set_difference(
        url_term_ids.begin(), url_term_ids.end(), term_id_set.begin(),
        term_id_set.end(),
        std::inserter(to_remove_terms, to_remove_terms.begin()));
    storage_->DeleteAugmentedTermIdsForUrl(to_remove_terms, url_id);
  }
  storage_->AddAugmentedTermIdsForUrl(term_id_set, url_id);
  return OpResults::kSuccess;
}

}  // namespace file_manager
