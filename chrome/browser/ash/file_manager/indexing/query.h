// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_QUERY_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_QUERY_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/file_manager/indexing/term.h"

namespace file_manager {

// Represents a parsed query.
class Query {
 public:
  explicit Query(const std::vector<Term>& terms);
  ~Query();

  // TODO(b:327535200): Reconsider copyability.
  Query(const Query& query);
  Query& operator=(const Query& other) = default;

  const std::vector<Term>& terms() const { return terms_; }

 private:
  std::vector<Term> terms_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_QUERY_H_
