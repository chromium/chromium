// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include "base/test/simple_test_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(DirtyBit, Constructor) {
  ASSERT_FALSE(DirtyBit());
  ASSERT_TRUE(DirtyBit(true));
  ASSERT_FALSE(DirtyBit(false));
}

TEST(DirtyBit, Assignment) {
  DirtyBit bit;

  bit = true;
  ASSERT_TRUE(bit);

  bit = false;
  ASSERT_FALSE(bit);
}

TEST(DirtyBit, Move) {
  DirtyBit bit(true);
  DirtyBit moved(std::move(bit));

  ASSERT_TRUE(moved);
  ASSERT_FALSE(bit);  // NOLINT
}

TEST(DIPSStateTest, GetSite) {
  EXPECT_EQ("example.com",
            DIPSStorage::GetSite(GURL("http://example.com/foo")));
  EXPECT_EQ("example.com",
            DIPSStorage::GetSite(GURL("https://www.example.com/bar")));
  EXPECT_EQ("example.com",
            DIPSStorage::GetSite(GURL("http://other.example.com/baz")));
  EXPECT_EQ("bar.baz.r.appspot.com",
            DIPSStorage::GetSite(GURL("http://foo.bar.baz.r.appspot.com/baz")));
  EXPECT_EQ("localhost",
            DIPSStorage::GetSite(GURL("http://localhost:8000/qux")));
  EXPECT_EQ("127.0.0.1", DIPSStorage::GetSite(GURL("http://127.0.0.1:8888/")));
  EXPECT_EQ("[::1]", DIPSStorage::GetSite(GURL("http://[::1]/")));
}

TEST(DIPSStateTest, NewURL) {
  DIPSStorage storage;
  DIPSState state = storage.Read(GURL("http://example.com/"));
  EXPECT_FALSE(state.was_loaded());
  EXPECT_FALSE(state.site_storage_time().has_value());
  EXPECT_FALSE(state.user_interaction_time().has_value());
}

TEST(DIPSStateTest, SetValues) {
  GURL url("https://example.com");
  auto time1 = absl::make_optional(base::Time::FromDoubleT(1));
  auto time2 = absl::make_optional(base::Time::FromDoubleT(2));
  DIPSStorage storage;

  {
    DIPSState state = storage.Read(url);
    state.set_site_storage_time(time1);
    state.set_user_interaction_time(time2);

    // Before flushing `state`, reads for the same URL won't include its
    // changes.
    DIPSState state2 = storage.Read(url);
    EXPECT_FALSE(state2.site_storage_time().has_value());
    EXPECT_FALSE(state2.user_interaction_time().has_value());
  }

  DIPSState state = storage.Read(url);
  EXPECT_TRUE(state.was_loaded());
  EXPECT_EQ(state.site_storage_time(), time1);
  EXPECT_EQ(state.user_interaction_time(), time2);
}

TEST(DIPSStateTest, SameSiteSameState) {
  // The two urls use different subdomains of example.com; and one is HTTPS
  // while the other is HTTP.
  GURL url1("https://subdomain1.example.com");
  GURL url2("http://subdomain2.example.com");
  auto time = absl::make_optional(base::Time::FromDoubleT(1));
  DIPSStorage storage;

  storage.Read(url1).set_site_storage_time(time);

  DIPSState state = storage.Read(url2);
  // State was recorded for url1, but can be read for url2.
  EXPECT_EQ(time, state.site_storage_time());
  EXPECT_FALSE(state.user_interaction_time().has_value());
}

TEST(DIPSStateTest, DifferentSiteDifferentState) {
  GURL url1("https://example1.com");
  GURL url2("https://example2.com");
  auto time1 = absl::make_optional(base::Time::FromDoubleT(1));
  auto time2 = absl::make_optional(base::Time::FromDoubleT(2));
  DIPSStorage storage;

  storage.Read(url1).set_site_storage_time(time1);
  storage.Read(url2).set_site_storage_time(time2);

  // Verify that url1 and url2 have independent state:
  EXPECT_EQ(storage.Read(url1).site_storage_time(), time1);
  EXPECT_EQ(storage.Read(url2).site_storage_time(), time2);
}
