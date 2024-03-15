// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_H_

#include <string>

namespace file_manager {

// Represents a term that can be associated with a file or used to query for a
// file. An example term would be a label given to a file. If the file has
// label "starred" associated with it, it would be represented by the
// Term("label", "starred") object. Other terms could be generated from the
// files's content, name, path, etc.
class Term {
 public:
  Term(const std::string& field, const std::u16string& text);
  ~Term();

  // TODO(b:327535200): Reconsider copyability.
  Term(const Term&) = default;
  Term& operator=(const Term&) = default;

  const std::string& field() const { return field_; }
  const std::string& text_bytes() const { return text_; }

 private:
  std::string field_;
  std::string text_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_TERM_H_
