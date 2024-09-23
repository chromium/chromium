// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_CLIENT_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_CLIENT_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A mock implementation of `HoldingSpaceClient` for use in testing.
class MockHoldingSpaceClient : public HoldingSpaceClient {
 public:
  MockHoldingSpaceClient();
  MockHoldingSpaceClient(const MockHoldingSpaceClient&) = delete;
  MockHoldingSpaceClient& operator=(const MockHoldingSpaceClient&) = delete;
  ~MockHoldingSpaceClient() override;

  // HoldingSpaceClient:
  MOCK_METHOD(const std::string&,
              AddItemOfType,
              (HoldingSpaceItem::Type type, const base::FilePath& file_path),
              (override));
  MOCK_METHOD(void,
              CopyImageToClipboard,
              (const HoldingSpaceItem& item,
               holding_space_metrics::EventSource event_source,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(base::FilePath,
              CrackFileSystemUrl,
              (const GURL& file_system_url),
              (const, override));
  MOCK_METHOD(bool, IsDriveDisabled, (), (const, override));
  MOCK_METHOD(void, OpenDownloads, (SuccessCallback callback), (override));
  MOCK_METHOD(void, OpenMyFiles, (SuccessCallback callback), (override));
  MOCK_METHOD(void,
              OpenItems,
              (const std::vector<const HoldingSpaceItem*>& items,
               holding_space_metrics::EventSource event_source,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              PinFiles,
              (const std::vector<base::FilePath>& file_paths,
               holding_space_metrics::EventSource event_source),
              (override));
  MOCK_METHOD(void, RefreshSuggestions, (), (override));
  MOCK_METHOD(void,
              RemoveSuggestions,
              (const std::vector<base::FilePath>& absolute_file_paths),
              (override));
  MOCK_METHOD(void,
              PinItems,
              (const std::vector<const HoldingSpaceItem*>& items,
               holding_space_metrics::EventSource event_source),
              (override));
  MOCK_METHOD(void,
              ShowItemInFolder,
              (const HoldingSpaceItem& item,
               holding_space_metrics::EventSource event_source,
               SuccessCallback callback),
              (override));
  MOCK_METHOD(void,
              UnpinItems,
              (const std::vector<const HoldingSpaceItem*>& items,
               holding_space_metrics::EventSource event_source),
              (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_MOCK_HOLDING_SPACE_CLIENT_H_
