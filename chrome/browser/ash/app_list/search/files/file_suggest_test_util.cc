// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_suggest_test_util.h"

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ash/app_list/search/files/file_suggest_util.h"

namespace app_list {

std::string CreateItemSuggestUpdateJsonString(
    const std::vector<SuggestItemMetadata>& item_data_array,
    const std::string& session_id) {
  base::Value::List list_value;
  for (const auto& data : item_data_array) {
    base::Value::Dict dict_value;
    dict_value.Set("itemId", data.item_id);
    dict_value.Set("displayText", data.display_text);
    dict_value.Set("predictionReason", data.prediction_reason);
    list_value.Append(std::move(dict_value));
  }

  base::Value::Dict suggest_item_update;
  suggest_item_update.Set("item", std::move(list_value));
  suggest_item_update.Set("suggestionSessionId", session_id);

  std::string json_string;
  base::JSONWriter::Write(suggest_item_update, &json_string);
  return json_string;
}

void WaitForFileSuggestionUpdate(
    const testing::NiceMock<MockFileSuggestKeyedServiceObserver>& mock,
    app_list::FileSuggestionType expected_type) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnFileSuggestionUpdated)
      .WillRepeatedly([&](app_list::FileSuggestionType type) {
        if (type == expected_type)
          run_loop.Quit();
      });
  run_loop.Run();
}

void WaitUntilFileSuggestServiceReady(FileSuggestKeyedService* service) {
  if (!service->IsReadyForTest()) {
    testing::NiceMock<MockFileSuggestKeyedServiceObserver> mock;
    base::ScopedObservation<app_list::FileSuggestKeyedService,
                            app_list::FileSuggestKeyedService::Observer>
        service_observer{&mock};
    service_observer.Observe(service);
    // Not sure which suggestion type is ready first. Therefore, wait for both.
    WaitForFileSuggestionUpdate(mock, FileSuggestionType::kDriveFile);
    if (service->IsReadyForTest())
      return;

    WaitForFileSuggestionUpdate(mock, FileSuggestionType::kLocalFile);
    EXPECT_TRUE(service->IsReadyForTest());
  }
}

}  // namespace app_list
