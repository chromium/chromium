// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/hash/hash.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_hstring.h"
#include "base/win/windows_version.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/win/fake_itoastnotification.h"
#include "chrome/browser/notifications/win/fake_notification_image_retainer.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/notifications/win/notification_template_builder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;

using message_center::Notification;

namespace {

constexpr char kLaunchId[] =
    "0|0|Default|0|https://example.com/|notification_id";
constexpr char kOrigin[] = "https://www.google.com/";
constexpr char kNotificationId[] = "id";
constexpr char kProfileId[] = "Default";

}  // namespace

class NotificationPlatformBridgeWinTest : public testing::Test {
 public:
  NotificationPlatformBridgeWinTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~NotificationPlatformBridgeWinTest() override = default;

 protected:
  mswr::ComPtr<winui::Notifications::IToastNotification2> GetToast(
      NotificationPlatformBridgeWin* bridge,
      const NotificationLaunchId& launch_id,
      bool renotify,
      const std::string& profile_id,
      bool incognito) {
    DCHECK(bridge);

    GURL origin(kOrigin);
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, L"title",
        L"message", gfx::Image(), L"display_source", origin,
        message_center::NotifierId(origin),
        message_center::RichNotificationData(), nullptr /* delegate */);
    notification->set_renotify(renotify);
    FakeNotificationImageRetainer image_retainer;
    base::string16 xml_template =
        BuildNotificationTemplate(&image_retainer, launch_id, *notification);

    mswr::ComPtr<winui::Notifications::IToastNotification> toast =
        bridge->GetToastNotificationForTesting(*notification, xml_template,
                                               profile_id, incognito);
    if (!toast) {
      LOG(ERROR) << "GetToastNotificationForTesting failed";
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
    HRESULT hr = toast.As<winui::Notifications::IToastNotification2>(&toast2);
    if (FAILED(hr)) {
      LOG(ERROR) << "Converting to IToastNotification2 failed";
      return nullptr;
    }

    return toast2;
  }

  content::BrowserTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWinTest);
};

TEST_F(NotificationPlatformBridgeWinTest, GroupAndTag) {
  // This test requires WinRT core functions, which are not available in
  // older versions of Windows.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  base::win::ScopedCOMInitializer com_initializer;

  NotificationPlatformBridgeWin bridge;

  NotificationLaunchId launch_id(kLaunchId);
  ASSERT_TRUE(launch_id.is_valid());

  mswr::ComPtr<winui::Notifications::IToastNotification2> toast2 =
      GetToast(&bridge, launch_id, /*renotify=*/false, kProfileId,
               /*incognito=*/false);
  ASSERT_TRUE(toast2);

  HSTRING hstring_group;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Group(&hstring_group));
  base::win::ScopedHString group(hstring_group);
  // NOTE: If you find yourself needing to change this value, make sure that
  // NotificationPlatformBridgeWinImpl::Close supports specifying the right
  // group value for RemoveGroupedTagWithId.
  ASSERT_STREQ(L"Notifications", group.Get().as_string().c_str());

  HSTRING hstring_tag;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Tag(&hstring_tag));
  base::win::ScopedHString tag(hstring_tag);
  std::string tag_data = std::string(kNotificationId) + "|" + kProfileId + "|0";
  ASSERT_STREQ(base::NumberToString16(base::Hash(tag_data)).c_str(),
               tag.Get().as_string().c_str());
}

TEST_F(NotificationPlatformBridgeWinTest, GroupAndTagUniqueness) {
  // This test requires WinRT core functions, which are not available in
  // older versions of Windows.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  base::win::ScopedCOMInitializer com_initializer;

  NotificationPlatformBridgeWin bridge;

  NotificationLaunchId launch_id(kLaunchId);
  ASSERT_TRUE(launch_id.is_valid());

  mswr::ComPtr<winui::Notifications::IToastNotification2> toastA;
  mswr::ComPtr<winui::Notifications::IToastNotification2> toastB;
  HSTRING hstring_tagA;
  HSTRING hstring_tagB;

  // Different profiles, same incognito status -> Unique tags.
  {
    toastA = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile2",
                      /*incognito=*/true);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_TRUE(tagA.Get().as_string() != tagB.Get().as_string());
  }

  // Same profile, different incognito status -> Unique tags.
  {
    toastA = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      /*incognito=*/false);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_TRUE(tagA.Get().as_string() != tagB.Get().as_string());
  }

  // Same profile, same incognito status -> Identical tags.
  {
    toastA = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      /*incognito=*/true);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_STREQ(tagA.Get().as_string().c_str(),
                 tagB.Get().as_string().c_str());
  }
}

TEST_F(NotificationPlatformBridgeWinTest, Suppress) {
  // This test requires WinRT core functions, which are not available in
  // older versions of Windows.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  base::win::ScopedCOMInitializer com_initializer;

  NotificationPlatformBridgeWin bridge;

  std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
      notifications;
  bridge.SetDisplayedNotificationsForTesting(&notifications);

  mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
  boolean suppress;

  NotificationLaunchId launch_id(kLaunchId);
  ASSERT_TRUE(launch_id.is_valid());

  // Make sure this works a toast is not suppressed when no notifications are
  // registered.
  toast2 = GetToast(&bridge, launch_id, /*renotify=*/false, kProfileId,
                    /*incognito=*/false);
  ASSERT_TRUE(toast2);
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_FALSE(suppress);
  toast2.Reset();

  // Register a single notification with a specific tag.
  std::string tag_data = std::string(kNotificationId) + "|" + kProfileId + "|0";
  base::string16 tag = base::NumberToString16(base::Hash(tag_data));
  // Microsoft::WRL::Make() requires FakeIToastNotification to derive from
  // RuntimeClass.
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      L"<toast launch=\"0|0|Default|0|https://foo.com/|id\"></toast>", tag));

  // Request this notification with renotify true (should not be suppressed).
  toast2 = GetToast(&bridge, launch_id, /*renotify=*/true, kProfileId,
                    /*incognito=*/false);
  ASSERT_TRUE(toast2);
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_FALSE(suppress);
  toast2.Reset();

  // Request this notification with renotify false (should be suppressed).
  toast2 = GetToast(&bridge, launch_id, /*renotify=*/false, kProfileId,
                    /*incognito=*/false);
  ASSERT_TRUE(toast2);
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_TRUE(suppress);
  toast2.Reset();

  bridge.SetDisplayedNotificationsForTesting(nullptr);
}
