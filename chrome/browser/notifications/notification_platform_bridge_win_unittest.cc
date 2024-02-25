// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_hstring.h"
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
    "0|0|Default|aumi|0|https://example.com/|notification_id";
constexpr char kOrigin[] = "https://www.google.com/";
constexpr char kNotificationId[] = "id";
constexpr char kProfileId[] = "Default";
constexpr wchar_t kAppUserModelId[] = L"aumi";
constexpr wchar_t kAppUserModelId2[] = L"aumi2";
constexpr char kAppUserModelIdUTF8[] = "aumi";

}  // namespace

class NotificationPlatformBridgeWinTest : public testing::Test {
 public:
  NotificationPlatformBridgeWinTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  NotificationPlatformBridgeWinTest(const NotificationPlatformBridgeWinTest&) =
      delete;
  NotificationPlatformBridgeWinTest& operator=(
      const NotificationPlatformBridgeWinTest&) = delete;

  ~NotificationPlatformBridgeWinTest() override = default;

 protected:
  mswr::ComPtr<winui::Notifications::IToastNotification2> GetToast(
      NotificationPlatformBridgeWin* bridge,
      const NotificationLaunchId& launch_id,
      bool renotify,
      const std::string& profile_id,
      const std::wstring& app_user_model_id,
      bool incognito) {
    DCHECK(bridge);

    GURL origin(kOrigin);
    auto notification = std::make_unique<message_center::Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, u"title",
        u"message", ui::ImageModel(), u"display_source", origin,
        message_center::NotifierId(origin),
        message_center::RichNotificationData(), nullptr /* delegate */);
    notification->set_renotify(renotify);
    FakeNotificationImageRetainer image_retainer;
    std::wstring xml_template =
        BuildNotificationTemplate(&image_retainer, launch_id, *notification);

    mswr::ComPtr<winui::Notifications::IToastNotification> toast =
        bridge->GetToastNotificationForTesting(*notification, xml_template,
                                               profile_id, app_user_model_id,
                                               incognito);
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
};

TEST_F(NotificationPlatformBridgeWinTest, GroupAndTag) {
  base::win::ScopedCOMInitializer com_initializer;

  NotificationPlatformBridgeWin bridge;

  NotificationLaunchId launch_id(kLaunchId);
  ASSERT_TRUE(launch_id.is_valid());

  mswr::ComPtr<winui::Notifications::IToastNotification2> toast2 =
      GetToast(&bridge, launch_id, /*renotify=*/false, kProfileId,
               kAppUserModelId, /*incognito=*/false);
  ASSERT_TRUE(toast2);

  HSTRING hstring_group;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Group(&hstring_group));
  base::win::ScopedHString group(hstring_group);
  // NOTE: If you find yourself needing to change this value, make sure that
  // NotificationPlatformBridgeWinImpl::Close supports specifying the right
  // group value for RemoveGroupedTagWithId.
  ASSERT_EQ(L"Notifications", group.Get());

  HSTRING hstring_tag;
  ASSERT_HRESULT_SUCCEEDED(toast2->get_Tag(&hstring_tag));
  base::win::ScopedHString tag(hstring_tag);
  std::string tag_data = std::string(kNotificationId) + "|" + kProfileId + "|" +
                         kAppUserModelIdUTF8 + "|0";
  ASSERT_EQ(base::NumberToWString(base::Hash(tag_data)), tag.Get());

  // Let tasks on |notification_task_runner_| of |bridge| run before its dtor.
  task_environment_.RunUntilIdle();
}

TEST_F(NotificationPlatformBridgeWinTest, GroupAndTagUniqueness) {
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
                      kAppUserModelId, /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile2",
                      kAppUserModelId, /*incognito=*/true);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_NE(tagA.Get(), tagB.Get());
  }

  // Same profile, different incognito status -> Unique tags.
  {
    toastA = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      kAppUserModelId, /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      kAppUserModelId, /*incognito=*/false);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_NE(tagA.Get(), tagB.Get());
  }

  // Same profile, same incognito status -> Identical tags.
  {
    toastA = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      kAppUserModelId, /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      kAppUserModelId, /*incognito=*/true);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_EQ(tagA.Get(), tagB.Get());
  }

  // Same profile, same incognito status, different app user model id
  // -> Unique tags.
  {
    toastA = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      kAppUserModelId, /*incognito=*/true);
    toastB = GetToast(&bridge, launch_id, /*renotify=*/false, "Profile1",
                      kAppUserModelId2, /*incognito=*/false);

    ASSERT_TRUE(toastA);
    ASSERT_TRUE(toastB);

    ASSERT_HRESULT_SUCCEEDED(toastA->get_Tag(&hstring_tagA));
    base::win::ScopedHString tagA(hstring_tagA);

    ASSERT_HRESULT_SUCCEEDED(toastB->get_Tag(&hstring_tagB));
    base::win::ScopedHString tagB(hstring_tagB);

    ASSERT_NE(tagA.Get(), tagB.Get());
  }

  // Let tasks on |notification_task_runner_| of |bridge| run before its dtor.
  task_environment_.RunUntilIdle();
}

TEST_F(NotificationPlatformBridgeWinTest, Suppress) {
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
                    kAppUserModelId, /*incognito=*/false);
  ASSERT_TRUE(toast2);
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_FALSE(suppress);
  toast2.Reset();

  // Register a single notification with a specific tag.
  std::string tag_data = std::string(kNotificationId) + "|" + kProfileId + "|" +
                         kAppUserModelIdUTF8 + "|0";
  std::wstring tag = base::NumberToWString(base::Hash(tag_data));
  // Microsoft::WRL::Make() requires FakeIToastNotification to derive from
  // RuntimeClass.
  notifications.push_back(Microsoft::WRL::Make<FakeIToastNotification>(
      L"<toast launch=\"0|0|Default|aumi|0|https://foo.com/|id\"></toast>",
      tag));

  // Request this notification with renotify true (should not be suppressed).
  toast2 = GetToast(&bridge, launch_id, /*renotify=*/true, kProfileId,
                    kAppUserModelId, /*incognito=*/false);
  ASSERT_TRUE(toast2);
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_FALSE(suppress);
  toast2.Reset();

  // Request this notification with renotify false (should be suppressed).
  toast2 = GetToast(&bridge, launch_id, /*renotify=*/false, kProfileId,
                    kAppUserModelId, /*incognito=*/false);
  ASSERT_TRUE(toast2);
  ASSERT_HRESULT_SUCCEEDED(toast2->get_SuppressPopup(&suppress));
  ASSERT_TRUE(suppress);
  toast2.Reset();

  // Let tasks on |notification_task_runner_| of |bridge| run before its dtor.
  task_environment_.RunUntilIdle();

  // Do this after we've finished running tasks to avoid touching
  // synchronize_displayed_notifications_timer_. See crbug.com/1220122.
  bridge.SetDisplayedNotificationsForTesting(nullptr);
}
