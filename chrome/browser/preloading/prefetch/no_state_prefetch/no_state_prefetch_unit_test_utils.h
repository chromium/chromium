// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_UNIT_TEST_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_UNIT_TEST_UTILS_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/preloading_data.h"
#include "url/gurl.h"

namespace prerender {

class UnitTestNoStatePrefetchManager;

class FakeNoStatePrefetchContents : public NoStatePrefetchContents {
 public:
  FakeNoStatePrefetchContents(
      UnitTestNoStatePrefetchManager* test_no_state_prefetch_manager,
      const GURL& url,
      Origin origin,
      const std::optional<url::Origin>& initiator_origin,
      FinalStatus expected_final_status);

  ~FakeNoStatePrefetchContents() override;

  void StartPrerendering(
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace,
      base::WeakPtr<content::PreloadingAttempt> preloading_attempt) override;

  FinalStatus expected_final_status() const { return expected_final_status_; }

  bool prefetching_has_been_cancelled() const {
    return NoStatePrefetchContents::prefetching_has_been_cancelled();
  }

 private:
  static int g_next_route_id_;
  int route_id_;

  raw_ptr<UnitTestNoStatePrefetchManager> test_no_state_prefetch_manager_;
  FinalStatus expected_final_status_;
};

class UnitTestNoStatePrefetchManager : public NoStatePrefetchManager {
 public:
  using NoStatePrefetchManager::kNavigationRecordWindowMs;

  explicit UnitTestNoStatePrefetchManager(Profile* profile);

  ~UnitTestNoStatePrefetchManager() override;

  // From KeyedService, via NoStatePrefetchManager:
  void Shutdown() override;

  // From NoStatePrefetchManager:
  void MoveEntryToPendingDelete(NoStatePrefetchContents* entry,
                                FinalStatus final_status) override;

  NoStatePrefetchContents* FindEntry(const GURL& url);

  std::unique_ptr<NoStatePrefetchContents> FindAndUseEntry(const GURL& url);

  FakeNoStatePrefetchContents* CreateNextNoStatePrefetchContents(
      const GURL& url,
      FinalStatus expected_final_status);

  FakeNoStatePrefetchContents* CreateNextNoStatePrefetchContents(
      const GURL& url,
      const std::optional<url::Origin>& initiator_origin,
      Origin origin,
      FinalStatus expected_final_status);

  FakeNoStatePrefetchContents* CreateNextNoStatePrefetchContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      FinalStatus expected_final_status);

  void set_rate_limit_enabled(bool enabled);

  NoStatePrefetchContents* next_no_state_prefetch_contents();

  NoStatePrefetchContents* GetNoStatePrefetchContentsForRoute(
      int child_id,
      int route_id) const override;

  void FakeNoStatePrefetchContentsStarted(
      int child_id,
      int route_id,
      NoStatePrefetchContents* no_state_prefetch_contents);

  void FakeNoStatePrefetchContentsDestroyed(int child_id, int route_id);

  void SetIsLowEndDevice(bool is_low_end_device) {
    is_low_end_device_ = is_low_end_device;
  }

 private:
  bool IsLowEndDevice() const override;

  FakeNoStatePrefetchContents* SetNextNoStatePrefetchContents(
      std::unique_ptr<FakeNoStatePrefetchContents> no_state_prefetch_contents);

  std::unique_ptr<NoStatePrefetchContents> CreateNoStatePrefetchContents(
      const GURL& url,
      const content::Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      Origin origin) override;

  // Maintain a map from route pairs to NoStatePrefetchContents for
  // GetNoStatePrefetchContentsForRoute.
  using NoStatePrefetchContentsMap =
      std::map<std::pair<int, int>,
               raw_ptr<NoStatePrefetchContents, CtnExperimental>>;
  NoStatePrefetchContentsMap no_state_prefetch_contents_map_;

  std::unique_ptr<NoStatePrefetchContents> next_no_state_prefetch_contents_;
  bool is_low_end_device_;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_NO_STATE_PREFETCH_UNIT_TEST_UTILS_H_
