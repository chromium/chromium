// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

void StoreContentSetting(ContentSetting* out_content_setting,
                         ContentSetting content_setting) {
  DCHECK(out_content_setting);
  *out_content_setting = content_setting;
}

class TestNotificationPermissionContext : public NotificationPermissionContext {
 public:
  explicit TestNotificationPermissionContext(Profile* profile)
      : NotificationPermissionContext(profile),
        permission_set_count_(0),
        last_permission_set_persisted_(false),
        last_permission_set_setting_(CONTENT_SETTING_DEFAULT) {}

  int permission_set_count() const { return permission_set_count_; }
  bool last_permission_set_persisted() const {
    return last_permission_set_persisted_;
  }
  ContentSetting last_permission_set_setting() const {
    return last_permission_set_setting_;
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    return HostContentSettingsMapFactory::GetForProfile(browser_context())
        ->GetContentSetting(url_a.DeprecatedGetOriginAsURL(),
                            url_b.DeprecatedGetOriginAsURL(),
                            content_settings_type());
  }

 private:
  // NotificationPermissionContext:
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override {
    permission_set_count_++;
    last_permission_set_persisted_ = persist;
    last_permission_set_setting_ = content_setting;
    NotificationPermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedder_origin, std::move(callback), persist,
        content_setting, /*is_one_time=*/false, is_final_decision);
  }

  int permission_set_count_;
  bool last_permission_set_persisted_;
  ContentSetting last_permission_set_setting_;
};

}  // namespace

class NotificationPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void TearDown() override {
    mock_time_task_runner_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::TestMockTimeTaskRunner* SwitchToMockTime() {
    EXPECT_FALSE(mock_time_task_runner_);
    mock_time_task_runner_ =
        std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();
    return mock_time_task_runner_->task_runner();
  }

  void UpdateContentSetting(NotificationPermissionContext* context,
                            const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting setting) {
    context->UpdateContentSetting(requesting_origin, embedding_origin, setting,
                                  /*is_one_time=*/false);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Registers the given |extension| with the extension service and returns the
  // extension if it could be registered appropriately.
  scoped_refptr<const extensions::Extension> RegisterExtension(
      scoped_refptr<const extensions::Extension> extension) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));

    extensions::ExtensionService* extension_service =
        test_extension_system->CreateExtensionService(
            &command_line, base::FilePath() /* install_directory */,
            false /* autoupdate_enabled */);

    extension_service->AddExtension(extension.get());

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());

    return base::WrapRefCounted(registry->GetExtensionById(
        extension->id(), extensions::ExtensionRegistry::ENABLED));
  }

  // Proxy to NotificationPermissionContext::GetPermissionStatusForExtension()
  // to avoid needing lots of FRIEND_TEST_ALL_PREFIXES declarations.
  ContentSetting GetPermissionStatusForExtension(
      const NotificationPermissionContext& context,
      const GURL& origin) const {
    return context.GetPermissionStatusForExtension(origin);
  }
#endif

 private:
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner>
      mock_time_task_runner_;
};

// Web Notification permission checks will never return ASK for cross-origin
// requests, as permission cannot be requested in that situation.
TEST_F(NotificationPermissionContextTest, CrossOriginPermissionChecks) {
  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");

  NotificationPermissionContext context(profile());

  // Both same-origin and cross-origin requests for |requesting_origin| should
  // have their default values.
  EXPECT_EQ(PermissionStatus::ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);

  // Now grant permission for the |requesting_origin|. This should be granted
  // in both contexts.
  UpdateContentSetting(&context, requesting_origin, requesting_origin,
                       CONTENT_SETTING_ALLOW);

  EXPECT_EQ(PermissionStatus::GRANTED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .status);

  EXPECT_EQ(PermissionStatus::GRANTED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);

  // Now block permission for |requesting_origin|.

#if BUILDFLAG(IS_ANDROID)
  // On Android O+, permission must be reset before it can be blocked. This is
  // because granting a permission on O+ creates a system-managed notification
  // channel which determines the value of the content setting, so it is not
  // allowed to then toggle the value from ALLOW->BLOCK directly. However,
  // Chrome may reset the permission (which deletes the channel), and *then*
  // grant/block it (creating a new channel).
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_OREO) {
    context.ResetPermission(requesting_origin, requesting_origin);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  UpdateContentSetting(&context, requesting_origin, requesting_origin,
                       CONTENT_SETTING_BLOCK);

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);

  // Resetting the permission should demonstrate the default behaviour again.
  context.ResetPermission(requesting_origin, requesting_origin);

  EXPECT_EQ(PermissionStatus::ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);
}

// Web Notifications permission requests should only succeed for top level
// origins (embedding origin == requesting origin). Retrieving previously
// granted permissions should continue to be possible regardless of being top
// level.
TEST_F(NotificationPermissionContextTest, WebNotificationsTopLevelOriginOnly) {
  GURL requesting_origin("https://example.com");
  GURL embedding_origin("https://chrome.com");

  NotificationPermissionContext context(profile());

  EXPECT_EQ(PermissionStatus::ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);

  // Requesting permission for different origins should fail.
  permissions::PermissionRequestID request_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId());

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  context.DecidePermission(
      permissions::PermissionRequestData(&context, request_id,
                                         /*user_gesture=*/true,
                                         requesting_origin, embedding_origin),
      base::BindOnce(&StoreContentSetting, &result));

  ASSERT_EQ(result, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(PermissionStatus::ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, requesting_origin)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);

  // Reading previously set permissions should continue to work.
  UpdateContentSetting(&context, requesting_origin, embedding_origin,
                       CONTENT_SETTING_ALLOW);

  EXPECT_EQ(PermissionStatus::GRANTED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     requesting_origin, embedding_origin)
                .status);

  context.ResetPermission(requesting_origin, embedding_origin);
}

// Web Notifications require secure origins.
TEST_F(NotificationPermissionContextTest, SecureOriginRequirement) {
  GURL insecure_origin("http://example.com");
  GURL secure_origin("https://chrome.com");

  NotificationPermissionContext web_notification_context(profile());

  EXPECT_EQ(PermissionStatus::DENIED,
            web_notification_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_origin, insecure_origin)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            web_notification_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_origin, secure_origin)
                .status);
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// Bulk-disabled for arm64 bot stabilization: https://crbug.com/1154345
#define MAYBE_TestDenyInIncognitoAfterDelay \
  DISABLED_TestDenyInIncognitoAfterDelay
#else
#define MAYBE_TestDenyInIncognitoAfterDelay TestDenyInIncognitoAfterDelay
#endif

// Tests auto-denial after a time delay in incognito.
TEST_F(NotificationPermissionContextTest, MAYBE_TestDenyInIncognitoAfterDelay) {
  TestNotificationPermissionContext permission_context(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  GURL url("https://www.example.com");
  NavigateAndCommit(url);

  const permissions::PermissionRequestID id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId());

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.RequestPermission(
      permissions::PermissionRequestData(&permission_context, id,
                                         /*user_gesture=*/true, url),
      base::DoNothing());

  // Should be blocked after 1-2 seconds, but the timer is reset whenever the
  // tab is not visible, so these 500ms never add up to >= 1 second.
  for (int n = 0; n < 10; n++) {
    web_contents()->WasShown();
    task_runner->FastForwardBy(base::Milliseconds(500));
    web_contents()->WasHidden();
  }

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // Time elapsed whilst hidden is not counted.
  // n.b. This line also clears out any old scheduled timer tasks. This is
  // important, because otherwise Timer::Reset (triggered by
  // VisibilityTimerTabHelper::WasShown) may choose to re-use an existing
  // scheduled task, and when it fires Timer::RunScheduledTask will call
  // TimeTicks::Now() (which unlike task_runner->NowTicks(), we can't fake),
  // and miscalculate the remaining delay at which to fire the timer.
  task_runner->FastForwardBy(base::Days(1));

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // Should be blocked after 1-2 seconds. So 500ms is not enough.
  web_contents()->WasShown();
  task_runner->FastForwardBy(base::Milliseconds(500));

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // But 5*500ms > 2 seconds, so it should now be blocked.
  for (int n = 0; n < 4; n++)
    task_runner->FastForwardBy(base::Milliseconds(500));

  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.last_permission_set_setting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetContentSettingFromMap(url, url));
}

// Tests how multiple parallel permission requests get auto-denied in incognito.
TEST_F(NotificationPermissionContextTest, TestParallelDenyInIncognito) {
  TestNotificationPermissionContext permission_context(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  GURL url("https://www.example.com");
  NavigateAndCommit(url);
  web_contents()->WasShown();

  const permissions::PermissionRequestID id1(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId(1));
  const permissions::PermissionRequestID id2(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId(2));

  base::TestMockTimeTaskRunner* task_runner = SwitchToMockTime();

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.RequestPermission(
      permissions::PermissionRequestData(&permission_context, id1,
                                         /*user_gesture=*/true, url),
      base::DoNothing());
  permission_context.RequestPermission(
      permissions::PermissionRequestData(&permission_context, id2,
                                         /*user_gesture=*/true, url),
      base::DoNothing());

  EXPECT_EQ(0, permission_context.permission_set_count());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context.GetContentSettingFromMap(url, url));

  // Fast forward up to 2.5 seconds. Stop as soon as the first permission
  // request is auto-denied.
  for (int n = 0; n < 5; n++) {
    task_runner->FastForwardBy(base::Milliseconds(500));
    if (permission_context.permission_set_count())
      break;
  }

  // Only the first permission request receives a response (crbug.com/577336).
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.last_permission_set_setting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetContentSettingFromMap(url, url));

  // After another 2.5 seconds, the second permission request should also have
  // received a response.
  task_runner->FastForwardBy(base::Milliseconds(2500));
  EXPECT_EQ(2, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.last_permission_set_setting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context.GetContentSettingFromMap(url, url));
}

TEST_F(NotificationPermissionContextTest, GetNotificationsSettings) {
  // Verifies that notification permissions, which don't store a secondary URL,
  // are stored appropriately in the HostContentSettingsMap.

  NotificationPermissionContext::UpdatePermission(
      profile(), GURL("https://allowed.com"), CONTENT_SETTING_ALLOW);
  NotificationPermissionContext::UpdatePermission(
      profile(), GURL("https://allowed2.com"), CONTENT_SETTING_ALLOW);

  NotificationPermissionContext::UpdatePermission(
      profile(), GURL("https://denied.com"), CONTENT_SETTING_BLOCK);
  NotificationPermissionContext::UpdatePermission(
      profile(), GURL("https://denied2.com"), CONTENT_SETTING_BLOCK);

  ContentSettingsForOneType settings =
      HostContentSettingsMapFactory::GetForProfile(profile())
          ->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS);

  // |settings| contains the default setting and 4 exceptions.
  ASSERT_EQ(5u, settings.size());

  // The platform isn't guaranteed to return the settings in any particular
  // order, so sort them first.
  std::sort(settings.begin(), settings.begin() + 4,
            [](const ContentSettingPatternSource& s1,
               const ContentSettingPatternSource& s2) {
              return s1.primary_pattern.GetHost() <
                     s2.primary_pattern.GetHost();
            });

  EXPECT_EQ(
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://allowed.com")),
      settings[0].primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, settings[0].GetContentSetting());
  EXPECT_EQ(
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://allowed2.com")),
      settings[1].primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, settings[1].GetContentSetting());
  EXPECT_EQ(
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://denied.com")),
      settings[2].primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, settings[2].GetContentSetting());
  EXPECT_EQ(
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://denied2.com")),
      settings[3].primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, settings[3].GetContentSetting());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), settings[4].primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ASK, settings[4].GetContentSetting());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(NotificationPermissionContextTest, ExtensionPermissionAskByDefault) {
  // Verifies that notification permission is not granted to extensions by
  // default. They need to explicitly declare this in their manifest.
  NotificationPermissionContext context(profile());

  scoped_refptr<const extensions::Extension> extension = RegisterExtension(
      extensions::ExtensionBuilder("Notification Permission Test").Build());

  ASSERT_TRUE(extension);

  ASSERT_EQ(CONTENT_SETTING_ASK,
            GetPermissionStatusForExtension(context, extension->url()));

  EXPECT_EQ(PermissionStatus::ASK,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     extension->url(), extension->url())
                .status);
}

TEST_F(NotificationPermissionContextTest, ExtensionPermissionGranted) {
  // Verifies that extensions that declare the "notifications" permission in
  // their manifest get notification permission granted.
  NotificationPermissionContext context(profile());

  scoped_refptr<const extensions::Extension> extension = RegisterExtension(
      extensions::ExtensionBuilder("Notification Permission Test")
          .AddAPIPermission("notifications")
          .Build());

  ASSERT_TRUE(extension);

  ASSERT_EQ(CONTENT_SETTING_ALLOW,
            GetPermissionStatusForExtension(context, extension->url()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     extension->url(), extension->url())
                .status);
}

TEST_F(NotificationPermissionContextTest, ExtensionPermissionOverrideDenied) {
  // Verifies that extensions that declare the "notifications" permission in
  // their manifest can still have permission disabled by the user.
  NotificationPermissionContext context(profile());

  scoped_refptr<const extensions::Extension> extension = RegisterExtension(
      extensions::ExtensionBuilder("Notification Permission Test")
          .AddAPIPermission("notifications")
          .Build());

  ASSERT_TRUE(extension);

  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(profile());
  DCHECK(notifier_state_tracker);

  // Disable the |extension|'s notification ability through the state tracker.
  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, extension->id());
  notifier_state_tracker->SetNotifierEnabled(notifier_id, /* enabled= */ false);

  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            GetPermissionStatusForExtension(context, extension->url()));

  EXPECT_EQ(PermissionStatus::DENIED,
            context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     extension->url(), extension->url())
                .status);
}
#endif
