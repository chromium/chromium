// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_CLIENT_UTIL_LOCAL_TAB_HANDLER_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_CLIENT_UTIL_LOCAL_TAB_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/input_delegate.h"

class Profile;

namespace segmentation_platform::processing {

// Implements TabFetcher for local tabs from tab strip models. Extends user data
// because this is intended to be stored as user data in segmentation service.
class LocalTabHandler : public TabFetcher, public base::SupportsUserData::Data {
 public:
  LocalTabHandler(sync_sessions::SessionSyncService* session_sync_service,
                  Profile* profile);
  ~LocalTabHandler() override;

  LocalTabHandler(const LocalTabHandler&) = delete;
  LocalTabHandler& operator=(const LocalTabHandler&) = delete;

  // TabFetcher impl.
  bool FillAllLocalTabsFromTabModel(std::vector<TabEntry>& tabs) override;
  Tab FindLocalTab(const TabEntry& entry) override;

 private:
  const raw_ptr<Profile> profile_;
};

// Tab session data source that adds info about local tabs.
class LocalTabSource : public TabSessionSource {
 public:
  LocalTabSource(sync_sessions::SessionSyncService* session_sync_service,
                 TabFetcher* tab_fetcher);
  ~LocalTabSource() override;

  LocalTabSource(const LocalTabSource&) = delete;
  LocalTabSource& operator=(const LocalTabSource&) = delete;

  // TabSessionSource impl.
  void AddLocalTabInfo(const TabFetcher::Tab& tab,
                       FeatureProcessorState& feature_processor_state,
                       Tensor& inputs) override;
};

}  // namespace segmentation_platform::processing

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_CLIENT_UTIL_LOCAL_TAB_HANDLER_H_
