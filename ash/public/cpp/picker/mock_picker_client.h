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
#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_PUBLIC_EXPORT MockPickerClient : public PickerClient {
 public:
  MockPickerClient();
  ~MockPickerClient() override;

  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetSharedURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(void,
              FetchGifSearch,
              (const std::string& query, FetchGifsCallback callback),
              (override));
  MOCK_METHOD(void, StopGifSearch, (), (override));
  MOCK_METHOD(void,
              StartCrosSearch,
              (const std::u16string& query,
               std::optional<PickerCategory> category,
               CrosSearchResultsCallback callback),
              (override));
  MOCK_METHOD(void, StopCrosQuery, (), (override));
  MOCK_METHOD(void, ShowEditor, (), (override));
  MOCK_METHOD(void,
              GetRecentLocalFileResults,
              (RecentFilesCallback),
              (override));
  MOCK_METHOD(void,
              GetRecentDriveFileResults,
              (RecentFilesCallback),
              (override));
  MOCK_METHOD(void,
              GetSuggestedLinkResults,
              (SuggestedLinksCallback),
              (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_MOCK_PICKER_CLIENT_H_
