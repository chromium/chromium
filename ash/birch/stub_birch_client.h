// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_STUB_BIRCH_CLIENT_H_
#define ASH_BIRCH_STUB_BIRCH_CLIENT_H_

#include "ash/birch/birch_client.h"
#include "ash/birch/birch_data_provider.h"
#include "base/files/scoped_temp_dir.h"

namespace ash {

// A BirchClient that returns data providers that do nothing for testing use.
class StubBirchClient : public BirchClient {
 public:
  // A data provider that does nothing.
  class StubDataProvider : public BirchDataProvider {
   public:
    StubDataProvider();
    StubDataProvider(const StubDataProvider&) = delete;
    StubDataProvider& operator=(const StubDataProvider&) = delete;
    ~StubDataProvider() override;

    bool did_request_birch_data_fetch() const {
      return did_request_birch_data_fetch_;
    }

    void RunDataProviderChangedCallback();

    // BirchDataProvider:
    void RequestBirchDataFetch() override;

   private:
    bool did_request_birch_data_fetch_ = false;
  };

  StubBirchClient();
  StubBirchClient(const StubBirchClient&) = delete;
  StubBirchClient& operator=(const StubBirchClient&) = delete;
  ~StubBirchClient() override;

  bool did_get_favicon_image() const { return did_get_favicon_image_; }
  bool did_wait_for_refresh_tokens() const {
    return did_wait_for_refresh_tokens_;
  }
  const base::FilePath& last_removed_path() const { return last_removed_path_; }

  // Installs a stub weather provider to birch model and returns the weather
  // data provider pointer.
  StubDataProvider* InstallStubWeatherDataProvider();
  // Installs a stub coral provider to birch model and returns the coral
  // data provider pointer.
  StubDataProvider* InstallStubCoralDataProvider();

  bool DidRequestCalendarDataFetch() const;
  bool DidRequestFileSuggestDataFetch() const;
  bool DidRequestRecentTabsDataFetch() const;
  bool DidRequestLastActiveDataFetch() const;
  bool DidRequestMostVisitedDataFetch() const;
  bool DidRequestSelfShareDataFetch() const;
  bool DidRequestLostMediaDataFetch() const;
  bool DidRequestReleaseNotesDataFetch() const;

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
  void RemoveFileItemFromLauncher(const base::FilePath& path) override;
  void GetFaviconImage(
      const GURL& url,
      const bool is_page_url,
      base::OnceCallback<void(const ui::ImageModel&)> callback) override;
  ui::ImageModel GetChromeBackupIcon() override;

 private:
  std::unique_ptr<StubDataProvider> calendar_provider_;
  std::unique_ptr<StubDataProvider> file_suggest_provider_;
  std::unique_ptr<StubDataProvider> recent_tabs_provider_;
  std::unique_ptr<StubDataProvider> last_active_provider_;
  std::unique_ptr<StubDataProvider> most_visited_provider_;
  std::unique_ptr<StubDataProvider> self_share_provider_;
  std::unique_ptr<StubDataProvider> lost_media_provider_;
  std::unique_ptr<StubDataProvider> release_notes_provider_;

  base::ScopedTempDir test_dir_;
  base::FilePath last_removed_path_;

  bool did_wait_for_refresh_tokens_ = false;
  bool did_get_favicon_image_ = false;
};

}  // namespace ash

#endif  // ASH_BIRCH_STUB_BIRCH_CLIENT_H_
