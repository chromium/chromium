// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_BLOCKLIST_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_BLOCKLIST_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/blocklist.h"
#include "chrome/browser/extensions/blocklist_state_fetcher.h"
#include "chrome/browser/extensions/scoped_database_manager_for_test.h"

namespace extensions {

class FakeSafeBrowsingDatabaseManager;

// Replace BlocklistStateFetcher for testing of the Boacklist class.
class BlocklistStateFetcherMock : public BlocklistStateFetcher {
 public:
  BlocklistStateFetcherMock();

  ~BlocklistStateFetcherMock() override;

  void Request(const std::string& id, RequestCallback callback) override;

  void SetState(const std::string& id, BlocklistState state);

  void Clear();

  int request_count() const { return request_count_; }

 private:
  std::map<std::string, BlocklistState> states_;
  int request_count_;
};

// A wrapper for an extensions::Blocklist that provides functionality for
// testing. It sets up mocks for SafeBrowsing database and BlocklistFetcher,
// that are used by blocklist to retrieve respectively the set of blocklisted
// extensions and their blocklist states.
class TestBlocklist {
 public:
  // Use this if the SafeBrowsing and/or StateFetcher mocks should be created
  // before initializing the Blocklist.
  TestBlocklist();

  explicit TestBlocklist(Blocklist* blocklist);

  TestBlocklist(const TestBlocklist&) = delete;
  TestBlocklist& operator=(const TestBlocklist&) = delete;

  ~TestBlocklist();

  void Attach(Blocklist* blocklist);

  // Only call this if Blocklist is destroyed before TestBlocklist, otherwise
  // it will be performed from the destructor.
  void Detach();

  Blocklist* blocklist() { return blocklist_; }

  // Set the extension state in SafeBrowsingDatabaseManager and
  // BlocklistFetcher.
  void SetBlocklistState(const std::string& extension_id,
                         BlocklistState state,
                         bool notify);

  BlocklistState GetBlocklistState(const std::string& extension_id);

  void Clear(bool notify);

  void DisableSafeBrowsing();

  void EnableSafeBrowsing();

  void NotifyUpdate();

  const BlocklistStateFetcherMock* fetcher() { return &state_fetcher_mock_; }

 private:
  raw_ptr<Blocklist> blocklist_;

  // The BlocklistStateFetcher object is normally managed by Blocklist. Because
  // of this, we need to prevent this object from being deleted with Blocklist.
  // For this, Detach() should be called before blocklist_ is deleted.
  BlocklistStateFetcherMock state_fetcher_mock_;

  scoped_refptr<FakeSafeBrowsingDatabaseManager> blocklist_db_;

  ScopedDatabaseManagerForTest scoped_blocklist_db_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_BLOCKLIST_H_
