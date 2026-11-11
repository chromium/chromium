// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_package.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/pickle.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/protocol/token.pb.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

TEST(TabStoragePackageTest, SerializePayloadMatchesProto) {
  tabs_pb::TabState tab_state;
  tab_state.set_tab_id(42);
  tab_state.set_parent_id(10);
  tab_state.set_timestamp_millis(1234567890);
  tab_state.set_url("https://example.com/test");
  tab_state.set_theme_color(0x112233);
  tab_state.set_launch_type_at_creation(2);
  tab_state.set_user_agent(1);
  tab_state.set_last_navigation_committed_timestamp_millis(1234567000);
  tab_state.set_tab_has_sensitive_content(true);
  tab_state.set_is_pinned(true);
  tab_state.set_opener_app_id("com.example.app");
  tab_state.set_web_contents_state_version(WebContentsState::kCurrentVersion);
  tab_state.set_web_contents_state_bytes("fake_serialized_bytes");

  tabs_pb::Token* tab_group_id = tab_state.mutable_tab_group_id();
  tab_group_id->set_high(0x1234ULL);
  tab_group_id->set_low(0x5678ULL);

  TabStoragePackage package(std::move(tab_state), nullptr);

  std::vector<uint8_t> payload = package.SerializePayload();
  ASSERT_FALSE(payload.empty());

  tabs_pb::TabState deserialized;
  ASSERT_TRUE(deserialized.ParseFromArray(payload.data(),
                                          static_cast<int>(payload.size())));

  EXPECT_EQ(deserialized.tab_id(), 42);
  EXPECT_EQ(deserialized.parent_id(), 10);
  EXPECT_EQ(deserialized.timestamp_millis(), 1234567890);
  EXPECT_EQ(deserialized.url(), "https://example.com/test");
  EXPECT_EQ(deserialized.theme_color(), 0x112233);
  EXPECT_EQ(deserialized.launch_type_at_creation(), 2);
  EXPECT_EQ(deserialized.user_agent(), 1);
  EXPECT_EQ(deserialized.last_navigation_committed_timestamp_millis(),
            1234567000);
  EXPECT_TRUE(deserialized.tab_has_sensitive_content());
  EXPECT_TRUE(deserialized.is_pinned());
  EXPECT_EQ(deserialized.opener_app_id(), "com.example.app");
  EXPECT_EQ(deserialized.web_contents_state_version(),
            WebContentsState::kCurrentVersion);
  EXPECT_EQ(deserialized.web_contents_state_bytes(), "fake_serialized_bytes");
  EXPECT_EQ(deserialized.tab_group_id().high(), 0x1234ULL);
  EXPECT_EQ(deserialized.tab_group_id().low(), 0x5678ULL);
}

TEST(TabStoragePackageTest, SerializePayloadWithPickle) {
  tabs_pb::TabState tab_state;
  tab_state.set_tab_id(101);
  tab_state.set_url("https://example.com/pickle");
  tab_state.set_web_contents_state_version(WebContentsState::kCurrentVersion);

  auto pickle = std::make_unique<base::Pickle>();
  pickle->WriteString(std::string(200, 'x'));
  const std::string expected_state_bytes(pickle->AsStringView());

  TabStoragePackage package(std::move(tab_state), std::move(pickle));

  std::vector<uint8_t> payload = package.SerializePayload();
  ASSERT_FALSE(payload.empty());

  tabs_pb::TabState deserialized;
  ASSERT_TRUE(deserialized.ParseFromArray(payload.data(),
                                          static_cast<int>(payload.size())));
  EXPECT_EQ(deserialized.tab_id(), 101);
  EXPECT_EQ(deserialized.url(), "https://example.com/pickle");
  EXPECT_EQ(deserialized.web_contents_state_version(),
            WebContentsState::kCurrentVersion);
  EXPECT_EQ(deserialized.web_contents_state_bytes(), expected_state_bytes);

  // The pickle has been moved into the proto; a second call must match.
  EXPECT_EQ(package.SerializePayload(), payload);
}

TEST(TabStoragePackageTest, SerializeChildrenReturnsEmpty) {
  tabs_pb::TabState tab_state;
  TabStoragePackage package(std::move(tab_state), nullptr);
  EXPECT_TRUE(package.SerializeChildren().empty());
}

TEST(TabStoragePackageTest, EmptyTabState) {
  tabs_pb::TabState tab_state;
  TabStoragePackage package(std::move(tab_state), nullptr);

  std::vector<uint8_t> payload = package.SerializePayload();
  tabs_pb::TabState deserialized;
  ASSERT_TRUE(deserialized.ParseFromArray(payload.data(),
                                          static_cast<int>(payload.size())));
  EXPECT_FALSE(deserialized.has_tab_id());
  EXPECT_FALSE(deserialized.has_url());
  EXPECT_FALSE(deserialized.has_web_contents_state_bytes());
}

}  // namespace tabs
