// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_TEST_BIRCH_CLIENT_H_
#define ASH_BIRCH_TEST_BIRCH_CLIENT_H_

#include <memory>
#include <vector>

#include "ash/birch/birch_client.h"
#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "base/files/scoped_temp_dir.h"
#include "ui/base/models/image_model.h"

namespace ash {
class BirchModel;

// A test birch data provider that runs the data fetched callback with saved
// items when receives a data fetch request.
template <typename T>
class TestBirchDataProvider : public BirchDataProvider {
 public:
  using DataFetchedCallback =
      base::RepeatingCallback<void(const std::vector<T>&)>;

  TestBirchDataProvider(DataFetchedCallback data_fetched_callback,
                        const std::string& pref_name);
  TestBirchDataProvider& operator=(const TestBirchDataProvider&) = delete;
  ~TestBirchDataProvider() override;

  void set_items(const std::vector<T>& items) { items_ = items; }

  void ClearItems();

  // Trigger data provider changed callback.
  void RunDataProviderChangedCallback();

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

 private:
  DataFetchedCallback data_fetched_callback_;
  const std::string pref_name_;
  std::vector<T> items_;
};

// A test birch client that returns the specific items to birch model.
class TestBirchClient : public BirchClient {
 public:
  explicit TestBirchClient(BirchModel* birch_model);
  TestBirchClient(const TestBirchClient&) = delete;
  TestBirchClient& operator=(const TestBirchClient&) = delete;
  ~TestBirchClient() override;

  void SetCalendarItems(const std::vector<BirchCalendarItem>& items);
  void SetFileSuggestItems(const std::vector<BirchFileItem>& items);
  void SetRecentTabsItems(const std::vector<BirchTabItem>& items);
  void SetLastActiveItems(const std::vector<BirchLastActiveItem>& items);
  void SetMostVisitedItems(const std::vector<BirchMostVisitedItem>& items);
  void SetReleaseNotesItems(const std::vector<BirchReleaseNotesItem>& items);
  void SetSelfShareItems(const std::vector<BirchSelfShareItem>& items);
  void SetLostMediaItems(const std::vector<BirchLostMediaItem>& items);

  // Clear all items.
  void Reset();

  // BirchClient:
  BirchDataProvider* GetCalendarProvider() override;
  BirchDataProvider* GetFileSuggestProvider() override;
  BirchDataProvider* GetRecentTabsProvider() override;
  BirchDataProvider* GetLastActiveProvider() override;
  BirchDataProvider* GetMostVisitedProvider() override;
  BirchDataProvider* GetSelfShareProvider() override;
  BirchDataProvider* GetLostMediaProvider() override;
  BirchDataProvider* GetReleaseNotesProvider() override;
  void WaitForRefreshTokens(base::OnceClosure callback) override;
  base::FilePath GetRemovedItemsFilePath() override;
  void RemoveFileItemFromLauncher(const base::FilePath& path) override {}
  void GetFaviconImage(
      const GURL& url,
      const bool is_page_url,
      base::OnceCallback<void(const ui::ImageModel&)> callback) override {}
  ui::ImageModel GetChromeBackupIcon() override;

 private:
  void HandleCalendarFetch(const std::vector<BirchCalendarItem>& items);

  std::unique_ptr<TestBirchDataProvider<BirchCalendarItem>> calendar_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchFileItem>> file_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchTabItem>> tab_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchLastActiveItem>>
      last_active_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchMostVisitedItem>>
      most_visited_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchSelfShareItem>>
      self_share_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchLostMediaItem>>
      lost_media_provider_;
  std::unique_ptr<TestBirchDataProvider<BirchReleaseNotesItem>>
      release_notes_provider_;

  base::ScopedTempDir test_dir_;
};

}  // namespace ash

#endif  // ASH_BIRCH_TEST_BIRCH_CLIENT_H_
