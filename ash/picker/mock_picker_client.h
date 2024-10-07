// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MOCK_PICKER_CLIENT_H_
#define ASH_PICKER_MOCK_PICKER_CLIENT_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_client.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;

namespace ash {

struct PickerWebPasteTarget;

class ASH_EXPORT MockPickerClient : public PickerClient {
 public:
  MockPickerClient();
  ~MockPickerClient() override;

  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetSharedURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(void,
              StartCrosSearch,
              (const std::u16string& query,
               std::optional<PickerCategory> category,
               CrosSearchResultsCallback callback),
              (override));
  MOCK_METHOD(void, StopCrosQuery, (), (override));
  MOCK_METHOD(bool, IsEligibleForEditor, (), (override));
  MOCK_METHOD(ShowEditorCallback, CacheEditorContext, (), (override));
  MOCK_METHOD(ShowLobsterCallback, GetShowLobsterCallback, (), (override));
  MOCK_METHOD(void,
              GetSuggestedEditorResults,
              (SuggestedEditorResultsCallback callback),
              (override));
  MOCK_METHOD(void,
              GetRecentLocalFileResults,
              (size_t, base::TimeDelta, RecentFilesCallback),
              (override));
  MOCK_METHOD(void,
              GetRecentDriveFileResults,
              (size_t, RecentFilesCallback),
              (override));
  MOCK_METHOD(void,
              GetSuggestedLinkResults,
              (size_t, SuggestedLinksCallback),
              (override));
  MOCK_METHOD(void,
              FetchFileThumbnail,
              (const base::FilePath& path,
               const gfx::Size& size,
               FetchFileThumbnailCallback callback),
              (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (override));
  MOCK_METHOD(std::optional<PickerWebPasteTarget>,
              GetWebPasteTarget,
              (),
              (override));
  MOCK_METHOD(void, Announce, (std::u16string_view message), (override));
};

}  // namespace ash

#endif  // ASH_PICKER_MOCK_PICKER_CLIENT_H_
