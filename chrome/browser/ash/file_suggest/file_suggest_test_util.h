// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_TEST_UTIL_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

/*
The suggest item metadata. It matches the json response used by
`ItemSuggestCache`. A sample json response is listed as below:
 R"(
    {
      "item": [
        {
          "itemId": "id",
          "displayText": "text",
          "predictionReason": "reason"
        }
      ],
      "suggestionSessionId": "session id"
    })";
*/
struct SuggestItemMetadata {
  std::string item_id;
  std::string display_text;
  std::string prediction_reason;
};

// Creates a json string used to update the item suggest cache.
std::string CreateItemSuggestUpdateJsonString(
    const std::vector<SuggestItemMetadata>& item_data_array,
    const std::string& session_id);

// Waits until `mock` is notified of the file suggestion update.
void WaitForFileSuggestionUpdate(
    const testing::NiceMock<MockFileSuggestKeyedServiceObserver>& mock,
    ash::FileSuggestionType expected_type);

// Waits until `service` is ready.
void WaitUntilFileSuggestServiceReady(ash::FileSuggestKeyedService* service);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_TEST_UTIL_H_
