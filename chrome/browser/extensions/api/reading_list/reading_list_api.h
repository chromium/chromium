// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_API_H_

#include "base/scoped_observation.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class ReadingListAddEntryFunction : public ExtensionFunction,
                                    public ReadingListModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.addEntry", READINGLIST_ADDENTRY)

  ReadingListAddEntryFunction();
  ReadingListAddEntryFunction(const ReadingListAddEntryFunction&) = delete;
  ReadingListAddEntryFunction& operator=(const ReadingListAddEntryFunction&) =
      delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListAddEntryFunction() override;

  ResponseValue AddEntryToReadingList();

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_observation_{this};
  raw_ptr<ReadingListModel> reading_list_model_;
  GURL url_;
  std::string title_;
  bool has_been_read_;
};

class ReadingListRemoveEntryFunction : public ExtensionFunction,
                                       public ReadingListModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.removeEntry", READINGLIST_REMOVEENTRY)

  ReadingListRemoveEntryFunction();
  ReadingListRemoveEntryFunction(const ReadingListRemoveEntryFunction&) =
      delete;
  ReadingListRemoveEntryFunction& operator=(
      const ReadingListRemoveEntryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListRemoveEntryFunction() override;

  ResponseValue RemoveEntryFromReadingList();

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_observation_{this};
  raw_ptr<ReadingListModel> reading_list_model_;
  GURL url_;
};

class ReadingListUpdateEntryFunction : public ExtensionFunction,
                                       public ReadingListModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.updateEntry", READINGLIST_UPDATEENTRY)

  ReadingListUpdateEntryFunction();
  ReadingListUpdateEntryFunction(const ReadingListUpdateEntryFunction&) =
      delete;
  ReadingListUpdateEntryFunction& operator=(
      const ReadingListUpdateEntryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListUpdateEntryFunction() override;

  ResponseValue UpdateEntriesInTheReadingList();

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_observation_{this};
  raw_ptr<ReadingListModel> reading_list_model_;
  GURL url_;
  std::optional<std::string> title_;
  std::optional<bool> has_been_read_;
};

class ReadingListQueryFunction : public ExtensionFunction,
                                 public ReadingListModelObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("readingList.query", READINGLIST_QUERY)

  ReadingListQueryFunction();
  ReadingListQueryFunction(const ReadingListQueryFunction&) = delete;
  ReadingListQueryFunction& operator=(const ReadingListQueryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~ReadingListQueryFunction() override;

  // Returns the entries that match the provided features.
  ResponseValue MatchEntries();

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_observation_{this};
  raw_ptr<ReadingListModel> reading_list_model_;
  std::optional<GURL> url_;
  std::optional<std::string> title_;
  std::optional<bool> has_been_read_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_API_H_
