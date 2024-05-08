// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_AUGMENTED_TERM_TABLE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_AUGMENTED_TERM_TABLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "sql/database.h"

namespace file_manager {

// Stores a mapping from augmented term IDs to an augmented term. The augmented
// term is a combination of a term and a field name. The main job of this table
// is to provide a unique ID for, say "label:starred" and "content:starred"
// terms. The term table provides a unique value for "starred". However, we
// need to be able to distinguish beteewn "starred" being used a, say, label,
// vs it being part of a content. This is what this table does.
class AugmentedTermTable {
 public:
  // Creates a table that maps augmented term IDs to augmented terms. An
  // augmented term consists of the field name and a term ID.
  explicit AugmentedTermTable(sql::Database* db);
  ~AugmentedTermTable();

  AugmentedTermTable(const AugmentedTermTable&) = delete;
  AugmentedTermTable& operator=(const AugmentedTermTable&) = delete;

  // Initializes the table. Returns true on success, and false on failure.
  bool Init();

  // Returns the ID corresponding to the given augmented term. If the augmented
  // term cannot be located, the method returns -1.
  int64_t GetAugmentedTermId(const std::string& field_name,
                             int64_t term_id) const;

  // Returns the ID corresponding to the augmented term. If the augmented term
  // cannot be located, a new ID is allocated and returned.
  int64_t GetOrCreateAugmentedTermId(const std::string& field_name,
                                     int64_t term_id);

  // Attempts to remove the given augmented term by its ID from the database.
  // If not present, this method returns -1. Otherwise, it returns the
  // `augmented_term_id`.
  int64_t DeleteAugmentedTermById(int64_t augmented_term_id);

 private:
  // The pointer to a database owned by the whoever created this table.
  raw_ptr<sql::Database> db_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_INDEXING_AUGMENTED_TERM_TABLE_H_
