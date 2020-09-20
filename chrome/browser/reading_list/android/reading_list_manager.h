// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

class ReadingListModel;

// Owns a reading list model and converts reading list data to bookmark nodes.
class ReadingListManager : public KeyedService {
 public:
  explicit ReadingListManager(ReadingListModel* reading_list_model);
  ~ReadingListManager() override;

  ReadingListManager(const ReadingListManager&) = delete;
  ReadingListManager& operator=(const ReadingListManager&) = delete;

 private:
  // Contains reading list data, outlives this class.
  ReadingListModel* reading_list_model_;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_
