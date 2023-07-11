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

  ResponseValue AddEntryToReadingList();

 private:
  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  ~ReadingListAddEntryFunction() override;

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_observation_{this};
  raw_ptr<ReadingListModel> reading_list_model_;
  GURL url_;
  std::string title_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_API_H_
