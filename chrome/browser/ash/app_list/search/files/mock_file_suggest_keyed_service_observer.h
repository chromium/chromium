// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_MOCK_FILE_SUGGEST_KEYED_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_MOCK_FILE_SUGGEST_KEYED_SERVICE_OBSERVER_H_

#include "chrome/browser/ash/app_list/search/files/file_suggest_keyed_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

// A mock implementation of `FileSuggestKeyedService::Observer` for testing.
class MockFileSuggestKeyedServiceObserver
    : public FileSuggestKeyedService::Observer {
 public:
  MockFileSuggestKeyedServiceObserver();
  MockFileSuggestKeyedServiceObserver(
      const MockFileSuggestKeyedServiceObserver&) = delete;
  MockFileSuggestKeyedServiceObserver& operator=(
      const MockFileSuggestKeyedServiceObserver&) = delete;
  ~MockFileSuggestKeyedServiceObserver() override;

  // app_list::FileSuggestKeyedService::Observer:
  MOCK_METHOD(void,
              OnFileSuggestionUpdated,
              (FileSuggestionType type),
              (override));
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_MOCK_FILE_SUGGEST_KEYED_SERVICE_OBSERVER_H_
