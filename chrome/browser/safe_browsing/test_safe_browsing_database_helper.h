// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_DATABASE_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_DATABASE_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/core/browser/db/util.h"

namespace safe_browsing {
class ListIdentifier;
class TestSafeBrowsingServiceFactory;
class TestV4GetHashProtocolManagerFactory;
}  // namespace safe_browsing

class InsertingDatabaseFactory;
class GURL;

// This class wraps a couple of safe browsing utilities that enable updating
// underlying SafeBrowsing lists to match URLs.
class TestSafeBrowsingDatabaseHelper {
 public:
  // Use this constructor for more in-depth customization of the database
  // helper. In particular, you can choose to:
  // 1. Send a nullptr protocol manager factory, so that hash requests are not
  //    mocked. Callers can consider mocking responses at the HTTP layer instead
  //    using StartRedirectingV4RequestsForTesting in
  //    v4_embedded_test_server_util.h.
  //
  // 2. Send a vector of additional lists to insert into the store map when
  //    initializing the test database. This allows lists which need chrome
  //    branding to function in non chrome branded tests (for developer
  //    ergonomics).
  TestSafeBrowsingDatabaseHelper(
      std::unique_ptr<safe_browsing::TestV4GetHashProtocolManagerFactory>
          v4_get_hash_factory,
      std::vector<safe_browsing::ListIdentifier> lists_to_insert);
  TestSafeBrowsingDatabaseHelper();

  TestSafeBrowsingDatabaseHelper(const TestSafeBrowsingDatabaseHelper&) =
      delete;
  TestSafeBrowsingDatabaseHelper& operator=(
      const TestSafeBrowsingDatabaseHelper&) = delete;

  ~TestSafeBrowsingDatabaseHelper();

  // Only compatible with the kMock policy. Marks the hash prefix for the URL as
  // bad in the local database and inserts it  into the full hash cache.
  void AddFullHashToDbAndFullHashCache(
      const GURL& bad_url,
      const safe_browsing::ListIdentifier& list_id,
      const safe_browsing::ThreatMetadata& threat_metadata);

  // Only marks the prefix as bad in the local database. Does not cache any full
  // hash response.
  void LocallyMarkPrefixAsBad(const GURL& url,
                              const safe_browsing::ListIdentifier& list_id);

  bool HasListSynced(const safe_browsing::ListIdentifier& list_id);

 private:
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory> sb_factory_;
  // Owned by the V4Database.
  raw_ptr<InsertingDatabaseFactory, AcrossTasksDanglingUntriaged>
      v4_db_factory_ = nullptr;

  // Owned by the V4GetHashProtocolManager. Will stay nullptr if the v4 hash
  // factory is not being mocked.
  raw_ptr<safe_browsing::TestV4GetHashProtocolManagerFactory,
          AcrossTasksDanglingUntriaged>
      v4_get_hash_factory_ = nullptr;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_DATABASE_HELPER_H_
