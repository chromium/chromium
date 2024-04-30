// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_unit_test_utils.h"

#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

int FakeNoStatePrefetchContents::g_next_route_id_ = 0;

FakeNoStatePrefetchContents::FakeNoStatePrefetchContents(
    UnitTestNoStatePrefetchManager* test_no_state_prefetch_manager,
    const GURL& url,
    Origin origin,
    const std::optional<url::Origin>& initiator_origin,
    FinalStatus expected_final_status)
    : NoStatePrefetchContents(
          std::make_unique<ChromeNoStatePrefetchContentsDelegate>(),
          test_no_state_prefetch_manager,
          nullptr,
          url,
          content::Referrer(),
          initiator_origin,
          origin),
      route_id_(g_next_route_id_++),
      test_no_state_prefetch_manager_(test_no_state_prefetch_manager),
      expected_final_status_(expected_final_status) {}

FakeNoStatePrefetchContents::~FakeNoStatePrefetchContents() {
  EXPECT_EQ(expected_final_status_, final_status());
  test_no_state_prefetch_manager_->FakeNoStatePrefetchContentsDestroyed(
      -1, route_id_);
}

void FakeNoStatePrefetchContents::StartPrerendering(
    const gfx::Rect& bounds,
    content::SessionStorageNamespace* session_storage_namespace,
    base::WeakPtr<content::PreloadingAttempt> preloading_attempt) {
  load_start_time_ = test_no_state_prefetch_manager_->GetCurrentTimeTicks();
  prefetching_has_started_ = true;
  test_no_state_prefetch_manager_->FakeNoStatePrefetchContentsStarted(
      -1, route_id_, this);
  NotifyPrefetchStart();
}

UnitTestNoStatePrefetchManager::UnitTestNoStatePrefetchManager(Profile* profile)
    : NoStatePrefetchManager(
          profile,
          std::make_unique<ChromeNoStatePrefetchManagerDelegate>(profile)) {
  set_rate_limit_enabled(false);
}

UnitTestNoStatePrefetchManager::~UnitTestNoStatePrefetchManager() {}

void UnitTestNoStatePrefetchManager::Shutdown() {
  if (next_no_state_prefetch_contents())
    next_no_state_prefetch_contents_->Destroy(FINAL_STATUS_PROFILE_DESTROYED);
  NoStatePrefetchManager::Shutdown();
}

void UnitTestNoStatePrefetchManager::MoveEntryToPendingDelete(
    NoStatePrefetchContents* entry,
    FinalStatus final_status) {
  if (entry == next_no_state_prefetch_contents_.get())
    return;
  NoStatePrefetchManager::MoveEntryToPendingDelete(entry, final_status);
}

NoStatePrefetchContents* UnitTestNoStatePrefetchManager::FindEntry(
    const GURL& url) {
  DeleteOldEntries();
  to_delete_prefetches_.clear();
  NoStatePrefetchData* data = FindNoStatePrefetchData(url, nullptr);
  return data ? data->contents() : nullptr;
}

std::unique_ptr<NoStatePrefetchContents>
UnitTestNoStatePrefetchManager::FindAndUseEntry(const GURL& url) {
  NoStatePrefetchData* no_state_prefetch_data =
      FindNoStatePrefetchData(url, nullptr);
  if (!no_state_prefetch_data)
    return nullptr;
  auto to_erase = FindIteratorForNoStatePrefetchContents(
      no_state_prefetch_data->contents());
  CHECK(to_erase != active_prefetches_.end());
  std::unique_ptr<NoStatePrefetchContents> no_state_prefetch_contents =
      no_state_prefetch_data->ReleaseContents();
  active_prefetches_.erase(to_erase);

  no_state_prefetch_contents->MarkAsUsedForTesting();
  return no_state_prefetch_contents;
}

FakeNoStatePrefetchContents*
UnitTestNoStatePrefetchManager::CreateNextNoStatePrefetchContents(
    const GURL& url,
    FinalStatus expected_final_status) {
  return SetNextNoStatePrefetchContents(
      std::make_unique<FakeNoStatePrefetchContents>(
          this, url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN,
          url::Origin::Create(GURL("https://uniquedifferentorigin.com")),
          expected_final_status));
}

FakeNoStatePrefetchContents*
UnitTestNoStatePrefetchManager::CreateNextNoStatePrefetchContents(
    const GURL& url,
    const std::optional<url::Origin>& initiator_origin,
    Origin origin,
    FinalStatus expected_final_status) {
  return SetNextNoStatePrefetchContents(
      std::make_unique<FakeNoStatePrefetchContents>(
          this, url, origin, initiator_origin, expected_final_status));
}

FakeNoStatePrefetchContents*
UnitTestNoStatePrefetchManager::CreateNextNoStatePrefetchContents(
    const GURL& url,
    const std::vector<GURL>& alias_urls,
    FinalStatus expected_final_status) {
  auto no_state_prefetch_contents =
      std::make_unique<FakeNoStatePrefetchContents>(
          this, url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN,
          url::Origin::Create(GURL("https://uniquedifferentorigin.com")),
          expected_final_status);
  for (const GURL& alias : alias_urls)
    EXPECT_TRUE(no_state_prefetch_contents->AddAliasURL(alias));
  return SetNextNoStatePrefetchContents(std::move(no_state_prefetch_contents));
}

void UnitTestNoStatePrefetchManager::set_rate_limit_enabled(bool enabled) {
  mutable_config().rate_limit_enabled = enabled;
}

NoStatePrefetchContents*
UnitTestNoStatePrefetchManager::next_no_state_prefetch_contents() {
  return next_no_state_prefetch_contents_.get();
}

NoStatePrefetchContents*
UnitTestNoStatePrefetchManager::GetNoStatePrefetchContentsForRoute(
    int child_id,
    int route_id) const {
  // Overridden for the NoStatePrefetchLinkManager's pending prefetch logic.
  auto it =
      no_state_prefetch_contents_map_.find(std::make_pair(child_id, route_id));
  return it != no_state_prefetch_contents_map_.end() ? it->second : nullptr;
}

void UnitTestNoStatePrefetchManager::FakeNoStatePrefetchContentsStarted(
    int child_id,
    int route_id,
    NoStatePrefetchContents* no_state_prefetch_contents) {
  no_state_prefetch_contents_map_[std::make_pair(child_id, route_id)] =
      no_state_prefetch_contents;
}

void UnitTestNoStatePrefetchManager::FakeNoStatePrefetchContentsDestroyed(
    int child_id,
    int route_id) {
  no_state_prefetch_contents_map_.erase(std::make_pair(child_id, route_id));
}

bool UnitTestNoStatePrefetchManager::IsLowEndDevice() const {
  return is_low_end_device_;
}

FakeNoStatePrefetchContents*
UnitTestNoStatePrefetchManager::SetNextNoStatePrefetchContents(
    std::unique_ptr<FakeNoStatePrefetchContents> no_state_prefetch_contents) {
  CHECK(!next_no_state_prefetch_contents_);
  FakeNoStatePrefetchContents* contents_ptr = no_state_prefetch_contents.get();
  next_no_state_prefetch_contents_ = std::move(no_state_prefetch_contents);
  return contents_ptr;
}

std::unique_ptr<NoStatePrefetchContents>
UnitTestNoStatePrefetchManager::CreateNoStatePrefetchContents(
    const GURL& url,
    const content::Referrer& referrer,
    const std::optional<url::Origin>& initiator_origin,
    Origin origin) {
  CHECK(next_no_state_prefetch_contents_);
  EXPECT_EQ(url, next_no_state_prefetch_contents_->prefetch_url());
  EXPECT_EQ(origin, next_no_state_prefetch_contents_->origin());
  return std::move(next_no_state_prefetch_contents_);
}

}  // namespace prerender
