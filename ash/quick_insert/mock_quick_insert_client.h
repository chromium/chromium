// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_MOCK_QUICK_INSERT_CLIENT_H_
#define ASH_QUICK_INSERT_MOCK_QUICK_INSERT_CLIENT_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/quick_insert/mock_quick_insert_client.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_client.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

struct QuickInsertWebPasteTarget;

class ASH_EXPORT MockQuickInsertClient : public QuickInsertClient {
 public:
  MockQuickInsertClient();
  ~MockQuickInsertClient() override;

  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetSharedURLLoaderFactory,
              (),
              (override));
  MOCK_METHOD(void,
              StartCrosSearch,
              (const std::u16string& query,
               std::optional<QuickInsertCategory> category,
               CrosSearchResultsCallback callback),
              (override));
  MOCK_METHOD(void, StopCrosQuery, (), (override));
  MOCK_METHOD(bool, IsEligibleForEditor, (), (override));
  MOCK_METHOD(ShowEditorCallback, CacheEditorContext, (), (override));
  MOCK_METHOD(ShowLobsterCallback,
              CacheLobsterContext,
              (ui::TextInputClient * text_input_client),
              (override));
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
              FetchFileThumbnail,
              (const base::FilePath& path,
               const gfx::Size& size,
               FetchFileThumbnailCallback callback),
              (override));
  MOCK_METHOD(std::optional<QuickInsertWebPasteTarget>,
              GetWebPasteTarget,
              (),
              (override));
  MOCK_METHOD(void, Announce, (std::u16string_view message), (override));
  MOCK_METHOD(history::HistoryService*, GetHistoryService, (), (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_MOCK_QUICK_INSERT_CLIENT_H_
