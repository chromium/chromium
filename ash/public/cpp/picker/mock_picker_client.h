// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_MOCK_PICKER_CLIENT_H_
#define ASH_PUBLIC_CPP_PICKER_MOCK_PICKER_CLIENT_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;

namespace ash {

class ASH_PUBLIC_EXPORT MockPickerClient : public PickerClient {
 public:
  MockPickerClient();
  ~MockPickerClient() override;

  MOCK_METHOD(void,
              StartCrosSearch,
              (const std::u16string& query,
               std::optional<PickerCategory> category,
               CrosSearchResultsCallback callback),
              (override));
  MOCK_METHOD(void, StopCrosQuery, (), (override));
  MOCK_METHOD(ShowEditorCallback, CacheEditorContext, (), (override));
  MOCK_METHOD(void,
              GetSuggestedEditorResults,
              (SuggestedEditorResultsCallback callback),
              (override));
  MOCK_METHOD(void,
              GetRecentLocalFileResults,
              (size_t, RecentFilesCallback),
              (override));
  MOCK_METHOD(void,
              GetRecentDriveFileResults,
              (size_t, RecentFilesCallback),
              (override));
  MOCK_METHOD(void,
              GetSuggestedLinkResults,
              (SuggestedLinksCallback),
              (override));
  MOCK_METHOD(bool, IsFeatureAllowedForDogfood, (), (override));
  MOCK_METHOD(void,
              FetchFileThumbnail,
              (const base::FilePath& path,
               const gfx::Size& size,
               FetchFileThumbnailCallback callback),
              (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_MOCK_PICKER_CLIENT_H_
