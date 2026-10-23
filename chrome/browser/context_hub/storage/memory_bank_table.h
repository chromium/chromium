// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_STORAGE_MEMORY_BANK_TABLE_H_
#define CHROME_BROWSER_CONTEXT_HUB_STORAGE_MEMORY_BANK_TABLE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace sql {
class Database;
}  // namespace sql

namespace context_hub {

// This class manages the tables storing memory bank entries and their
// embeddings within the SQLite database passed to initialization.
//
// Schema:
// -----------------------------------------------------------------------------
// memory_bank_entries
//
//   id                                 INTEGER PRIMARY KEY Auto-incremented by
//                                      SQLite, uniquely identifies each entry.
//   type                               INTEGER NOT NULL (0: kTab,
//                                      1: kTextSelection, 2: kImage)
//   timestamp                          INTEGER NOT NULL The timestamp of the
//                                      memory bank entry, stored as
//                                      microseconds since the Windows epoch.
//   url                                TEXT NOT NULL The URL of the memory bank
//                                      entry.
//   tab_title                          TEXT NOT NULL The title of the tab
//                                      associated with the memory bank entry.
//   selected_text                      TEXT The selected text from the page.
//   tags                               TEXT (JSON-serialized string of tags)
//   note                               TEXT User-provided notes for the entry.
//   collection                         TEXT The collection name the entry
//                                      belongs to.
//   image_url                          TEXT The source URL of the image when
//                                      the entry is of type kImage.
//
// memory_bank_embeddings
//
//   entry_id                           INTEGER PRIMARY KEY NOT NULL The id of
//                                      the memory_bank_entries row these
//                                      embeddings were computed from.
//   embedding_blob                     BLOB NOT NULL All passage embedding
//                                      vectors for the entry: a native-endian
//                                      uint32 dimension followed by each
//                                      vector's floats, packed back to back.
//                                      The vector count is (blob size - 4) /
//                                      (4 * dimension). Changing this format
//                                      requires a migration that clears this
//                                      table so embeddings are recomputed.
//
// memory_bank_embeddings_meta
//
//   name                               TEXT PRIMARY KEY NOT NULL The metadata
//                                      key, e.g. "embeddings_version".
//   value                              INTEGER NOT NULL The metadata value.
// -----------------------------------------------------------------------------
class MemoryBankTable {
 public:
  MemoryBankTable();
  MemoryBankTable(const MemoryBankTable&) = delete;
  MemoryBankTable& operator=(const MemoryBankTable&) = delete;
  ~MemoryBankTable();

  // Initializes the table with the given SQLite database. Must be called
  // before any other methods. Returns true on success.
  bool Init(sql::Database* db);

  // Creates the memory_bank_entries table and its indexes at database
  // version 1. Should only be called when initializing or migrating a database
  // from a clean state.
  bool MigrateFromCleanStateToVersion1();

  // Migrates the memory_bank_entries table from version 1 to version 2 by
  // adding the note and collection columns. Returns true on success.
  bool MigrateToVersion2AddNoteAndCollectionColumns();

  // Migrates the memory_bank_entries table from version 2 to version 3 by
  // adding the image_url column. Returns true on success.
  bool MigrateToVersion3AddImageUrlColumn();

  // Migrates the database to version 4 by creating the memory_bank_embeddings
  // and memory_bank_embeddings_meta tables. Returns true on success.
  bool MigrateToVersion4AddEmbeddingsTables();

  // Inserts or replaces a record in memory_bank_entries. Returns the entry's
  // ID on success, or std::nullopt on failure.
  std::optional<int64_t> AddOrUpdateEntry(const MemoryBankEntry& entry);

  // Updates the annotations (tags, note, collection) for an existing entry.
  // Returns true on success, or false if not found or on error.
  bool UpdateEntryAnnotations(int64_t id,
                              const std::vector<std::string>& tags,
                              const std::optional<std::string>& note,
                              const std::optional<std::string>& collection);

  // Inserts or replaces the passage embeddings for the given entry ID. Returns
  // true on success; returns false if `embeddings` is empty, the embeddings do
  // not all share the same non-zero dimension, or the entry does not exist.
  bool AddOrUpdateEmbeddings(
      int64_t entry_id,
      base::span<const passage_embeddings::Embedding> embeddings);

  // Retrieves the IDs of all memory_bank_entries rows that do not yet have a
  // corresponding memory_bank_embeddings row.
  std::vector<int64_t> GetEntryIdsWithoutEmbeddings();

  // Returns the version under which the stored embeddings were computed, or
  // std::nullopt if none has been set.
  std::optional<int64_t> GetEmbeddingsVersion();

  // Records `version` as the embeddings version. If it differs from the stored
  // version, all stored embeddings are deleted in the same transaction, since
  // embeddings from different versions are not comparable. No-op if
  // unchanged. Returns true on success.
  bool SetEmbeddingsVersion(int64_t version);

  // Retrieves a single entry by ID. Returns std::nullopt if not found.
  std::optional<MemoryBankEntry> GetEntry(int64_t id);

  // Retrieves entries matching the given IDs.
  std::vector<MemoryBankEntry> GetEntriesByIds(base::span<const int64_t> ids);

  // Retrieves all entries from memory_bank_entries ordered by timestamp DESC.
  std::vector<MemoryBankEntry> GetAllEntries();

  // Retrieves all unique tags stored across memory bank entries.
  std::vector<std::string> GetAllTags();

  // Retrieves all unique collection names stored across memory bank entries.
  std::vector<std::string> GetAllCollections();

  // Returns the total number of entries in memory_bank_entries.
  size_t GetEntryCount();

  // Deletes records by IDs. Returns true on success.
  bool DeleteEntries(base::span<const int64_t> ids);

 private:
  // Owned by ContextHubDatabase, outlives this class.
  raw_ptr<sql::Database> db_ = nullptr;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_STORAGE_MEMORY_BANK_TABLE_H_
