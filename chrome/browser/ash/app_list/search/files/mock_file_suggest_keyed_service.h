// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_MOCK_FILE_SUGGEST_KEYED_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_MOCK_FILE_SUGGEST_KEYED_SERVICE_H_

#include <map>

#include "chrome/browser/ash/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ash/app_list/search/files/file_suggest_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

namespace base {
class FilePath;
}  // namespace base

namespace app_list {

// A mock file suggestion service. Simply returns the cached suggestions without
// relying on real suggestion providers.
class MockFileSuggestKeyedService : public app_list::FileSuggestKeyedService {
 public:
  static std::unique_ptr<KeyedService> BuildMockFileSuggestKeyedService(
      const base::FilePath& proto_path,
      content::BrowserContext* context);

  MockFileSuggestKeyedService(Profile* profile,
                              PersistentProto<RemovedResultsProto> proto);
  MockFileSuggestKeyedService(const MockFileSuggestKeyedService&) = delete;
  MockFileSuggestKeyedService& operator=(const MockFileSuggestKeyedService&) =
      delete;
  ~MockFileSuggestKeyedService() override;

  // app_list::FileSuggestKeyedService:
  void GetSuggestFileData(app_list::FileSuggestionType type,
                          GetSuggestFileDataCallback callback) override;
  MOCK_METHOD(void,
              RemoveSuggestionsAndNotify,
              (const std::vector<base::FilePath>& suggested_file_paths),
              (override));
  MOCK_METHOD(void,
              RemoveSuggestionBySearchResultAndNotify,
              (const ash::SearchResultMetadata& search_result),
              (override));

  void SetSuggestionsForType(
      app_list::FileSuggestionType type,
      const absl::optional<std::vector<app_list::FileSuggestData>>&
          suggestions);

 private:
  void RunGetSuggestFileDataCallback(app_list::FileSuggestionType type,
                                     GetSuggestFileDataCallback callback);

  // Caches file suggestions.
  std::map<app_list::FileSuggestionType,
           absl::optional<std::vector<app_list::FileSuggestData>>>
      type_suggestion_mappings_;

  base::WeakPtrFactory<MockFileSuggestKeyedService> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_MOCK_FILE_SUGGEST_KEYED_SERVICE_H_
