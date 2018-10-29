// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_launch_id.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NotificationLaunchIdTest, SerializationTests) {
  {
    NotificationLaunchId id(NotificationHandler::Type::WEB_PERSISTENT,
                            "notification_id", "Default", true,
                            GURL("https://example.com"));
    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ("0|0|Default|1|https://example.com/|notification_id",
              id.Serialize());
  }

  {
    NotificationLaunchId id(NotificationHandler::Type::WEB_PERSISTENT,
                            "notification_id", "Default", true,
                            GURL("https://example.com"));
    id.set_button_index(0);
    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ("1|0|0|Default|1|https://example.com/|notification_id",
              id.Serialize());
  }

  {
    NotificationLaunchId id(NotificationHandler::Type::WEB_PERSISTENT,
                            "notification_id", "Default", true,
                            GURL("https://example.com"));
    id.set_is_for_context_menu();
    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ("2|0|Default|1|https://example.com/|notification_id",
              id.Serialize());
  }

  {
    NotificationLaunchId id(NotificationHandler::Type::WEB_PERSISTENT,
                            "notification_id", "Default", true,
                            GURL("https://example.com"));
    id.set_is_for_dismiss_button();
    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ("3|0|Default|1|https://example.com/|notification_id",
              id.Serialize());
  }
}

TEST(NotificationLaunchIdTest, ParsingTests) {
  // Input string as Windows passes it to us when you click on the notification.
  {
    std::string encoded = "0|0|Default|1|https://example.com/|notification_id";
    NotificationLaunchId id(encoded);

    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ(-1, id.button_index());
    EXPECT_FALSE(id.is_for_context_menu());
    EXPECT_FALSE(id.is_for_dismiss_button());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id", id.notification_id());
  }

  // Extra pipe signs should be treated as part of the notification id.
  {
    std::string encoded =
        "0|0|Default|1|https://example.com/|notification_id|Extra|Data";
    NotificationLaunchId id(encoded);

    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ(-1, id.button_index());
    EXPECT_FALSE(id.is_for_context_menu());
    EXPECT_FALSE(id.is_for_dismiss_button());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id|Extra|Data", id.notification_id());
  }

  // Input string for when a button is pressed.
  {
    std::string encoded =
        "1|0|0|Default|1|https://example.com/|notification_id";
    NotificationLaunchId id(encoded);

    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ(0, id.button_index());
    EXPECT_FALSE(id.is_for_context_menu());
    EXPECT_FALSE(id.is_for_dismiss_button());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id", id.notification_id());
  }

  // Input string for when a button is pressed, with extra pipes in notification
  // id.
  {
    std::string encoded =
        "1|0|0|Default|1|https://example.com/|notification_id|Extra|Data|";
    NotificationLaunchId id(encoded);

    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ(0, id.button_index());
    EXPECT_FALSE(id.is_for_context_menu());
    EXPECT_FALSE(id.is_for_dismiss_button());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id|Extra|Data|", id.notification_id());
  }

  // Input string for when the context menu item is selected.
  {
    std::string encoded = "2|0|Default|1|https://example.com/|notification_id";
    NotificationLaunchId id(encoded);

    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ(-1, id.button_index());
    EXPECT_TRUE(id.is_for_context_menu());
    EXPECT_FALSE(id.is_for_dismiss_button());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id", id.notification_id());
  }

  // Input string for when the context menu item is selected.
  {
    std::string encoded = "3|0|Default|1|https://example.com/|notification_id";
    NotificationLaunchId id(encoded);

    ASSERT_TRUE(id.is_valid());
    EXPECT_EQ(-1, id.button_index());
    EXPECT_FALSE(id.is_for_context_menu());
    EXPECT_TRUE(id.is_for_dismiss_button());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id", id.notification_id());
  }
}

TEST(NotificationLaunchIdTest, ParsingErrorCases) {
  struct TestCases {
    const char* const encoded_string;
  } cases[] = {
      {""},
      // Missing button index/notification type.
      {"1|0|Default|1|https://example.com/|notification_id"},
      // Valid, except button index is not an int.
      {"1|a|0|Default|1|https://example.com/|notification_id"},
      // Missing notification id from end.
      {"0|0|Default|1|https://example.com/"},
      // Missing notification id, and origin.
      {"0|0|Default|1"},
      // Missing notification id, origin, and incognito.
      {"0|0|Default"},
      // Missing notification id, origin, incognito, and profile id.
      {"0|0"},
      // Missing all but the component type (type NORMAL).
      {"0"},
      // Missing all but the component type (type BUTTON_INDEX).
      {"1"},
      // Missing all but the component type (type CONTEXT_MENU).
      {"2"},
      // Missing all but the component type (type DISMISS_BUTTON).
      {"3"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.encoded_string);
    NotificationLaunchId id(test_case.encoded_string);
    EXPECT_FALSE(id.is_valid());
  }
}

TEST(NotificationLaunchIdTest, GetProfileIdFromLaunchId) {
  // Given a valid launch id, the profile id can be obtained correctly.
  ASSERT_EQ(NotificationLaunchId::GetProfileIdFromLaunchId(
                L"1|1|0|Default|0|https://example.com/|notification_id"),
            "Default");

  // Given an invalid launch id, the profile id is set to an empty string.
  ASSERT_EQ(NotificationLaunchId::GetProfileIdFromLaunchId(
                L"1|Default|0|https://example.com/|notification_id"),
            "");
}
