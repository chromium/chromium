// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/dev_mode_bubble_delegate.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/proxy_overridden_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_bubble_delegate.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_delegate.h"
#include "chrome/browser/extensions/test_extension_message_bubble_delegate.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace {

const char kId1[] = "iccfkkhkfiphcjdakkmcjmkfboccmndk";
const char kId2[] = "ajjhifimiemdpmophmkkkcijegphclbl";
const char kId3[] = "ioibbbfddncmmabjmpokikkeiofalaek";

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<extensions::ExtensionWebUIOverrideRegistrar>(context);
}

// Creates a new ToolbarActionsModel for the given |context|.
std::unique_ptr<KeyedService> BuildToolbarModel(
    content::BrowserContext* context) {
  return std::make_unique<ToolbarActionsModel>(
      Profile::FromBrowserContext(context),
      extensions::ExtensionPrefs::Get(context));
}

}  // namespace

namespace extensions {

class TestExtensionMessageBubbleController :
    public ExtensionMessageBubbleController {
 public:
  TestExtensionMessageBubbleController(
      ExtensionMessageBubbleController::Delegate* delegate,
      Browser* browser)
      : ExtensionMessageBubbleController(delegate, browser),
        action_button_callback_count_(0),
        dismiss_button_callback_count_(0),
        link_click_callback_count_(0) {}

  TestExtensionMessageBubbleController(
      const TestExtensionMessageBubbleController&) = delete;
  TestExtensionMessageBubbleController& operator=(
      const TestExtensionMessageBubbleController&) = delete;

  ~TestExtensionMessageBubbleController() override {}

  // ExtensionMessageBubbleController:
  void OnBubbleAction() override {
    ++action_button_callback_count_;
    ExtensionMessageBubbleController::OnBubbleAction();
  }
  void OnBubbleDismiss(bool by_deactivation) override {
    ++dismiss_button_callback_count_;
    ExtensionMessageBubbleController::OnBubbleDismiss(by_deactivation);
  }
  void OnLinkClicked() override {
    ++link_click_callback_count_;
    ExtensionMessageBubbleController::OnLinkClicked();
  }

  size_t action_click_count() { return action_button_callback_count_; }
  size_t dismiss_click_count() { return dismiss_button_callback_count_; }
  size_t link_click_count() { return link_click_callback_count_; }

 private:
  // How often each button has been called.
  size_t action_button_callback_count_;
  size_t dismiss_button_callback_count_;
  size_t link_click_callback_count_;
};

// A fake bubble used for testing the controller. Takes an action that specifies
// what should happen when the bubble is "shown" (the bubble is actually not
// shown, the corresponding action is taken immediately).
class FakeExtensionMessageBubble {
 public:
  enum ExtensionBubbleAction {
    BUBBLE_ACTION_CLICK_ACTION_BUTTON = 0,
    BUBBLE_ACTION_CLICK_DISMISS_BUTTON,
    BUBBLE_ACTION_DISMISS_DEACTIVATION,
    BUBBLE_ACTION_CLICK_LINK,
    BUBBLE_ACTION_IGNORE,
  };

  FakeExtensionMessageBubble()
      : is_closed_(true),
        action_(BUBBLE_ACTION_CLICK_ACTION_BUTTON),
        controller_(nullptr) {}

  FakeExtensionMessageBubble(const FakeExtensionMessageBubble&) = delete;
  FakeExtensionMessageBubble& operator=(const FakeExtensionMessageBubble&) =
      delete;

  void set_action_on_show(ExtensionBubbleAction action) {
    action_ = action;
  }
  void set_controller(ExtensionMessageBubbleController* controller) {
    controller_ = controller;
  }

  bool is_closed() { return is_closed_; }

  void Show() {
    controller_->OnShown(base::BindOnce(&FakeExtensionMessageBubble::Close,
                                        base::Unretained(this)));

    // Depending on the user action, the bubble may be closed as result.
    switch (action_) {
      case BUBBLE_ACTION_CLICK_ACTION_BUTTON:
        controller_->OnBubbleAction();
        is_closed_ = true;
        break;
      case BUBBLE_ACTION_CLICK_DISMISS_BUTTON:
        controller_->OnBubbleDismiss(false);
        is_closed_ = true;
        break;
      case BUBBLE_ACTION_DISMISS_DEACTIVATION:
        controller_->OnBubbleDismiss(true);
        is_closed_ = true;
        break;
      case BUBBLE_ACTION_CLICK_LINK:
        controller_->OnLinkClicked();
        // Opening a new tab for the learn more link can cause the bubble to
        // close.
        is_closed_ = true;
        break;
      case BUBBLE_ACTION_IGNORE:
        is_closed_ = false;
        break;
    }
  }

 private:
  // Dummy close callback.
  void Close() { is_closed_ = true; }

  bool is_closed_;
  ExtensionBubbleAction action_;
  raw_ptr<ExtensionMessageBubbleController, DanglingUntriaged> controller_;
};

class ExtensionMessageBubbleTest : public BrowserWithTestWindowTest {
 public:
  ExtensionMessageBubbleTest() {}

  testing::AssertionResult LoadGenericExtension(const std::string& index,
                                                const std::string& id,
                                                ManifestLocation location) {
    ExtensionBuilder builder;
    builder.SetManifest(base::Value::Dict()
                            .Set("name", std::string("Extension " + index))
                            .Set("version", "1.0")
                            .Set("manifest_version", 2));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionWithAction(const std::string& index,
                                                   const std::string& id,
                                                   ManifestLocation location) {
    ExtensionBuilder builder;
    builder.SetManifest(
        base::Value::Dict()
            .Set("name", std::string("Extension " + index))
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("browser_action",
                 base::Value::Dict().Set("default_title", "Default title")));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingHome(
      const std::string& index,
      const std::string& id,
      ManifestLocation location) {
    ExtensionBuilder builder;
    builder.SetManifest(
        base::Value::Dict()
            .Set("name", std::string("Extension " + index))
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("chrome_settings_overrides",
                 base::Value::Dict().Set("homepage", "http://www.google.com")));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingStart(
      const std::string& index,
      const std::string& id,
      ManifestLocation location) {
    ExtensionBuilder builder;
    builder.SetManifest(
        base::Value::Dict()
            .Set("name", std::string("Extension " + index))
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("chrome_settings_overrides",
                 base::Value::Dict().Set(
                     "startup_pages",
                     base::Value::List().Append("http://www.google.com"))));
    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingNtp(
      const std::string& index,
      const std::string& id,
      ManifestLocation location) {
    ExtensionBuilder builder;
    builder.SetManifest(
        base::Value::Dict()
            .Set("name", std::string("Extension " + index))
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("chrome_url_overrides",
                 base::Value::Dict().Set("newtab", "Default.html")));

    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  testing::AssertionResult LoadExtensionOverridingProxy(
      const std::string& index,
      const std::string& id,
      ManifestLocation location) {
    ExtensionBuilder builder;
    builder.SetManifest(
        base::Value::Dict()
            .Set("name", std::string("Extension " + index))
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("permissions", base::Value::List().Append("proxy")));

    builder.SetLocation(location);
    builder.SetID(id);
    service_->AddExtension(builder.Build().get());

    // The proxy check relies on ExtensionPrefValueMap being up to date as to
    // specifying which extension is controlling the proxy, but unfortunately
    // that Map is not updated automatically for unit tests, so we simulate the
    // update here to avoid test failures.
    ExtensionPrefValueMap* extension_prefs_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(profile());
    extension_prefs_value_map->RegisterExtension(
        id,
        base::Time::Now(),
        true,    // is_enabled.
        false);  // is_incognito_enabled.
    extension_prefs_value_map->SetExtensionPref(id, proxy_config::prefs::kProxy,
                                                kExtensionPrefsScopeRegular,
                                                base::Value(id));

    if (ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(id))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << "Could not install extension: " << id;
  }

  void Init() {
    LoadErrorReporter::Init(false);
    // The two lines of magical incantation required to get the extension
    // service to work inside a unit test and access the extension prefs.
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))
        ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                 base::FilePath(), false);
    service_ = ExtensionSystem::Get(profile())->extension_service();
    service_->Init();

    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()
        ->SetTestingFactory(profile(),
                            base::BindRepeating(&BuildOverrideRegistrar));
    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(
        profile());
    ToolbarActionsModelFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildToolbarModel));
  }

  ExtensionMessageBubbleTest(const ExtensionMessageBubbleTest&) = delete;
  ExtensionMessageBubbleTest& operator=(const ExtensionMessageBubbleTest&) =
      delete;

  ~ExtensionMessageBubbleTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    command_line_ =
        std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
    ExtensionMessageBubbleController::set_should_ignore_learn_more_for_testing(
        true);
    // Prevent the Profile from getting deleted before TearDown() is complete,
    // since WaitForStorageCleanup() relies on an active Profile. See the
    // DestroyProfileOnBrowserClose flag.
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile(), ProfileKeepAliveOrigin::kBrowserWindow);
  }

  void TearDown() override {
    ExtensionMessageBubbleController::set_should_ignore_learn_more_for_testing(
        false);
    WaitForStorageCleanup();
    // Clean up global state for the delegates. Since profiles are stored in
    // global variables, they can be shared between tests and cause
    // unpredicatable behavior.
    DevModeBubbleDelegate(profile()).ClearProfileSetForTesting();
    ProxyOverriddenBubbleDelegate(profile()).ClearProfileSetForTesting();
    for (auto type : {BUBBLE_TYPE_HOME_PAGE, BUBBLE_TYPE_SEARCH_ENGINE,
                      BUBBLE_TYPE_STARTUP_PAGES}) {
      SettingsApiBubbleDelegate(profile(), type).ClearProfileSetForTesting();
    }
    SuspiciousExtensionBubbleDelegate(profile()).ClearProfileSetForTesting();
    profile_keep_alive_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void ShowAndDismissBubbleByDeactivation(
      TestExtensionMessageBubbleController* controller,
      const std::string& extension_name) {
    controller->SetIsActiveBubble();
    EXPECT_TRUE(controller->ShouldShow());
    std::vector<std::u16string> override_extensions =
        controller->GetExtensionList();
    ASSERT_EQ(1U, override_extensions.size());
    EXPECT_EQ(base::UTF8ToUTF16(extension_name), override_extensions[0]);
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    EXPECT_EQ(0U, controller->action_click_count());

    // Simulate showing the bubble and dismissing it by clicking outside of the
    // bubble.
    FakeExtensionMessageBubble bubble;
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_DISMISS_DEACTIVATION);
    EXPECT_TRUE(controller->ShouldShow());
    bubble.set_controller(controller);
    bubble.Show();
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(0U, controller->action_click_count());
    EXPECT_EQ(1U, controller->dismiss_click_count());

    // Since no action was taken, the bubble should still be showable.
    EXPECT_FALSE(controller->ShouldShow());
  }

  void WaitForStorageCleanup() {
    content::StoragePartition* partition =
        profile()->GetDefaultStoragePartition();
    if (partition)
      partition->WaitForDeletionTasksForTesting();
  }

 protected:
  raw_ptr<ExtensionService, DanglingUntriaged> service_;

 private:
  std::unique_ptr<base::CommandLine> command_line_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

// Test that the bubble correctly treats dismissal due to deactivation.
TEST_F(ExtensionMessageBubbleTest,
       BubbleDoesNotAcknowledgeExtensionOnDeactivationDismissal) {
  Init();

  scoped_refptr<const Extension> extension = ExtensionBuilder("Alpha").Build();
  service_->AddExtension(extension.get());
  auto test_delegate = std::make_unique<TestExtensionMessageBubbleDelegate>(
      browser()->profile());
  test_delegate->IncludeExtensionId(extension->id());

  auto* test_delegate_raw = test_delegate.get();
  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      test_delegate.release(), browser());

  controller->SetIsActiveBubble();

  // The list will contain the single extension.
  EXPECT_TRUE(controller->ShouldShow());
  std::vector<std::u16string> listed_extensions =
      controller->GetExtensionList();
  ASSERT_EQ(1U, listed_extensions.size());
  EXPECT_EQ(u"Alpha", listed_extensions[0]);
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_EQ(0U, controller->action_click_count());

  // Simulate showing the bubble and dismissing it due to deactivation.
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_DISMISS_DEACTIVATION);
  bubble.set_controller(controller.get());
  bubble.Show();
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());

  // Since the bubble was dismissed due to deactivation, the extension should
  // not have been acknowledged.
  EXPECT_FALSE(test_delegate_raw->WasExtensionAcknowledged(extension->id()));
}

// The feature this is meant to test is only enacted on Windows, but it should
// pass on all platforms.
TEST_F(ExtensionMessageBubbleTest, WipeoutControllerTest) {
  Init();
  // Add three extensions, and control two of them in this test (extension 1
  // and 2).
  ASSERT_TRUE(
      LoadExtensionWithAction("1", kId1, ManifestLocation::kCommandLine));
  ASSERT_TRUE(LoadGenericExtension("2", kId2, ManifestLocation::kUnpacked));
  ASSERT_TRUE(
      LoadGenericExtension("3", kId3, ManifestLocation::kExternalPolicy));

  std::unique_ptr<TestExtensionMessageBubbleController> controller(
      new TestExtensionMessageBubbleController(
          new SuspiciousExtensionBubbleDelegate(browser()->profile()),
          browser()));
  controller->SetIsActiveBubble();
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);

  // Validate that we don't have a suppress value for the extensions.
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));

  EXPECT_FALSE(controller->ShouldShow());
  std::vector<std::u16string> suspicious_extensions =
      controller->GetExtensionList();
  EXPECT_EQ(0U, suspicious_extensions.size());
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());

  // Now disable an extension, specifying the wipeout flag.
  service_->DisableExtension(kId1, disable_reason::DISABLE_NOT_VERIFIED);

  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new SuspiciousExtensionBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  controller->delegate()->ClearProfileSetForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(1U, suspicious_extensions.size());
  EXPECT_EQ(u"Extension 1", suspicious_extensions[0]);
  bubble.set_controller(controller.get());
  bubble.Show();  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  // Now the acknowledge flag should be set only for the first extension.
  EXPECT_TRUE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
  // Clear the flag.
  controller->delegate()->SetBubbleInfoBeenAcknowledged(kId1, false);
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));

  // Now disable the other extension and exercise the link click code path.
  service_->DisableExtension(kId2, disable_reason::DISABLE_NOT_VERIFIED);

  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new SuspiciousExtensionBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  controller->delegate()->ClearProfileSetForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(2U, suspicious_extensions.size());
  EXPECT_EQ(u"Extension 1", suspicious_extensions[1]);
  EXPECT_EQ(u"Extension 2", suspicious_extensions[0]);
  bubble.set_controller(controller.get());
  bubble.Show();  // Simulate showing the bubble.
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
}

// The feature this is meant to test is only enacted on Windows, but it should
// pass on all platforms.
TEST_F(ExtensionMessageBubbleTest, DevModeControllerTest) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
  Init();
  // Add three extensions, and control two of them in this test (extension 1
  // and 2). Extension 1 is a regular extension, Extension 2 is UNPACKED so it
  // counts as a DevMode extension.
  ASSERT_TRUE(
      LoadExtensionWithAction("1", kId1, ManifestLocation::kCommandLine));
  ASSERT_TRUE(LoadGenericExtension("2", kId2, ManifestLocation::kUnpacked));
  ASSERT_TRUE(
      LoadGenericExtension("3", kId3, ManifestLocation::kExternalPolicy));

  std::unique_ptr<TestExtensionMessageBubbleController> controller(
      new TestExtensionMessageBubbleController(
          new DevModeBubbleDelegate(browser()->profile()), browser()));
  controller->SetIsActiveBubble();

  // The list will contain one enabled unpacked extension.
  EXPECT_TRUE(controller->ShouldShow());
  std::vector<std::u16string> dev_mode_extensions =
      controller->GetExtensionList();
  ASSERT_EQ(2U, dev_mode_extensions.size());
  EXPECT_EQ(u"Extension 2", dev_mode_extensions[0]);
  EXPECT_EQ(u"Extension 1", dev_mode_extensions[1]);
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_EQ(0U, controller->action_click_count());

  // Simulate showing the bubble.
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
  bubble.set_controller(controller.get());
  bubble.Show();
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != nullptr);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != nullptr);

  // Do it again, but now press different button (Disable).
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new DevModeBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  // Most bubbles would want to show again as long as the extensions weren't
  // acknowledged and the bubble wasn't dismissed due to deactivation. Since dev
  // mode extensions can't be (persistently) acknowledged, this isn't the case
  // for the dev mode bubble, and we should only show once per profile.
  EXPECT_FALSE(controller->ShouldShow());
  controller->delegate()->ClearProfileSetForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(2U, dev_mode_extensions.size());
  bubble.set_controller(controller.get());
  bubble.Show();  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId1) != nullptr);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != nullptr);

  // Re-enable the extensions (disabled by the action button above).
  service_->EnableExtension(kId1);
  service_->EnableExtension(kId2);

  // Now disable the unpacked extension.
  service_->DisableExtension(kId1, disable_reason::DISABLE_USER_ACTION);
  service_->DisableExtension(kId2, disable_reason::DISABLE_USER_ACTION);

  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new DevModeBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  controller->delegate()->ClearProfileSetForTesting();
  EXPECT_FALSE(controller->ShouldShow());
  dev_mode_extensions = controller->GetExtensionList();
  EXPECT_EQ(0U, dev_mode_extensions.size());
}

// Test that if we show the dev mode bubble for the regular profile, we won't
// show it for its incognito profile.
// Regression test for crbug.com/819309.
TEST_F(ExtensionMessageBubbleTest, ShowDevModeBubbleOncePerOriginalProfile) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
  Init();

  ASSERT_TRUE(LoadGenericExtension("1", kId1, ManifestLocation::kUnpacked));

  auto get_controller = [](Browser* browser) {
    auto controller = std::make_unique<TestExtensionMessageBubbleController>(
        new DevModeBubbleDelegate(browser->profile()), browser);
    controller->SetIsActiveBubble();
    return controller;
  };

  {
    // Show the bubble for the regular profile, and dismiss it.
    auto controller = get_controller(browser());
    EXPECT_TRUE(controller->ShouldShow());
    FakeExtensionMessageBubble bubble;
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
    bubble.set_controller(controller.get());
    bubble.Show();
  }

  {
    // The bubble shouldn't want to show twice for the same profile.
    auto controller = get_controller(browser());
    EXPECT_FALSE(controller->ShouldShow());
  }

  {
    // Construct an off-the-record profile and browser.
    Profile* off_the_record_profile =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

    ToolbarActionsModelFactory::GetInstance()->SetTestingFactory(
        off_the_record_profile, base::BindRepeating(&BuildToolbarModel));

    std::unique_ptr<BrowserWindow> off_the_record_window(CreateBrowserWindow());
    std::unique_ptr<Browser> off_the_record_browser(
        CreateBrowser(off_the_record_profile, Browser::TYPE_NORMAL, false,
                      off_the_record_window.get()));

    // The bubble shouldn't want to show for an incognito version of the same
    // profile.
    auto controller = get_controller(browser());
    EXPECT_FALSE(controller->ShouldShow());

    // Now, try the inverse - show the bubble for the incognito profile, and
    // dismiss it.
    controller->delegate()->ClearProfileSetForTesting();
    EXPECT_TRUE(controller->ShouldShow());
    FakeExtensionMessageBubble bubble;
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
    bubble.set_controller(controller.get());
    bubble.Show();
  }

  {
    // The bubble shouldn't want to show for the regular profile.
    auto controller = get_controller(browser());
    EXPECT_FALSE(controller->ShouldShow());
  }
}

// The feature this is meant to test is only implemented on Windows and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

TEST_F(ExtensionMessageBubbleTest, SettingsApiControllerTest) {
  Init();

  for (int i = 0; i < 3; ++i) {
    switch (static_cast<SettingsApiOverrideType>(i)) {
      case BUBBLE_TYPE_HOME_PAGE:
        // Load two extensions overriding home page and one overriding something
        // unrelated (to check for interference). Extension 2 should still win
        // on the home page setting.
        ASSERT_TRUE(LoadExtensionOverridingHome("1", kId1,
                                                ManifestLocation::kUnpacked));
        ASSERT_TRUE(LoadExtensionOverridingHome("2", kId2,
                                                ManifestLocation::kUnpacked));
        ASSERT_TRUE(LoadExtensionOverridingStart("3", kId3,
                                                 ManifestLocation::kUnpacked));
        break;
      case BUBBLE_TYPE_SEARCH_ENGINE:
        // We deliberately skip testing the search engine since it relies on
        // TemplateURLServiceFactory that isn't available while unit testing.
        // This test is only simulating the bubble interaction with the user and
        // that is more or less the same for the search engine as it is for the
        // others.
        continue;
      case BUBBLE_TYPE_STARTUP_PAGES:
        // Load two extensions overriding start page and one overriding
        // something unrelated (to check for interference). Extension 2 should
        // still win on the startup page setting.
        ASSERT_TRUE(LoadExtensionOverridingStart("1", kId1,
                                                 ManifestLocation::kUnpacked));
        ASSERT_TRUE(LoadExtensionOverridingStart("2", kId2,
                                                 ManifestLocation::kUnpacked));
        ASSERT_TRUE(LoadExtensionOverridingHome("3", kId3,
                                                ManifestLocation::kUnpacked));
        break;
      default:
        NOTREACHED();
        break;
    }

    SettingsApiOverrideType type = static_cast<SettingsApiOverrideType>(i);
    std::unique_ptr<TestExtensionMessageBubbleController> controller(
        new TestExtensionMessageBubbleController(
            new SettingsApiBubbleDelegate(browser()->profile(), type),
            browser()));
    controller->SetIsActiveBubble();

    // The list will contain one enabled unpacked extension (ext 2).
    EXPECT_TRUE(controller->ShouldShow());
    std::vector<std::u16string> override_extensions =
        controller->GetExtensionList();
    ASSERT_EQ(1U, override_extensions.size());
    EXPECT_EQ(u"Extension 2", override_extensions[0]);
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    EXPECT_EQ(0U, controller->action_click_count());

    // Simulate showing the bubble and dismissing it.
    FakeExtensionMessageBubble bubble;
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
    bubble.set_controller(controller.get());
    bubble.Show();
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(0U, controller->action_click_count());
    EXPECT_EQ(1U, controller->dismiss_click_count());
    // No extension should have become disabled.
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
    // Only extension 2 should have been acknowledged.
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
    EXPECT_TRUE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId3));
    // Clean up after ourselves.
    controller->delegate()->SetBubbleInfoBeenAcknowledged(kId2, false);

    // Simulate clicking the learn more link to dismiss it.
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
    controller = std::make_unique<TestExtensionMessageBubbleController>(
        new SettingsApiBubbleDelegate(browser()->profile(), type), browser());
    controller->SetIsActiveBubble();
    bubble.set_controller(controller.get());
    bubble.Show();
    EXPECT_EQ(1U, controller->link_click_count());
    EXPECT_EQ(0U, controller->action_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    // No extension should have become disabled.
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
    // Only extension 2 should have been acknowledged.
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
    EXPECT_TRUE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId3));
    // Clean up after ourselves.
    controller->delegate()->SetBubbleInfoBeenAcknowledged(kId2, false);

    // Do it again, but now opt to disable the extension.
    bubble.set_action_on_show(
        FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
    controller = std::make_unique<TestExtensionMessageBubbleController>(
        new SettingsApiBubbleDelegate(browser()->profile(), type), browser());
    controller->SetIsActiveBubble();
    EXPECT_TRUE(controller->ShouldShow());
    override_extensions = controller->GetExtensionList();
    EXPECT_EQ(1U, override_extensions.size());
    bubble.set_controller(controller.get());
    bubble.Show();  // Simulate showing the bubble.
    EXPECT_EQ(0U, controller->link_click_count());
    EXPECT_EQ(1U, controller->action_click_count());
    EXPECT_EQ(0U, controller->dismiss_click_count());
    // Only extension 2 should have become disabled.
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != NULL);
    EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != NULL);
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != NULL);
    // No extension should have been acknowledged (it got disabled).
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
    EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId3));

    // Clean up after ourselves.
    service_->UninstallExtension(kId1,
                                 extensions::UNINSTALL_REASON_FOR_TESTING,
                                 NULL);
    service_->UninstallExtension(kId2,
                                 extensions::UNINSTALL_REASON_FOR_TESTING,
                                 NULL);
    service_->UninstallExtension(kId3,
                                 extensions::UNINSTALL_REASON_FOR_TESTING,
                                 NULL);
  }
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Tests that a displayed extension bubble will be closed after its associated
// enabled extension is uninstalled.
TEST_F(ExtensionMessageBubbleTest, BubbleClosedAfterEnabledExtensionUninstall) {
  Init();

  scoped_refptr<const Extension> extension = ExtensionBuilder("Alpha").Build();
  service_->AddExtension(extension.get());
  auto test_delegate = std::make_unique<TestExtensionMessageBubbleDelegate>(
      browser()->profile());
  test_delegate->IncludeExtensionId(extension->id());

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      test_delegate.release(), browser());
  controller->SetIsActiveBubble();

  EXPECT_TRUE(controller->ShouldShow());
  ASSERT_EQ(1U, controller->GetExtensionList().size());

  // Simulate showing the bubble and take no action.
  FakeExtensionMessageBubble bubble;
  EXPECT_TRUE(controller->ShouldShow());
  bubble.set_controller(controller.get());
  bubble.set_action_on_show(FakeExtensionMessageBubble::BUBBLE_ACTION_IGNORE);
  bubble.Show();
  EXPECT_FALSE(bubble.is_closed());

  // Uninstall the extension.
  service_->UninstallExtension(extension->id(), UNINSTALL_REASON_FOR_TESTING,
                               nullptr);
  ASSERT_EQ(0U, controller->GetExtensionList().size());

  // The bubble should be closed after the extension is uninstalled.
  EXPECT_TRUE(bubble.is_closed());

  controller.reset();
}

// Tests that a displayed extension bubble will be closed after its associated
// disabled extension is uninstalled. Here a suspicious bubble controller is
// tested, which can display bubbles for disabled extensions.
TEST_F(ExtensionMessageBubbleTest,
       BubbleClosedAfterDisabledExtensionUninstall) {
  Init();
  ASSERT_TRUE(
      LoadExtensionOverridingNtp("1", kId1, ManifestLocation::kCommandLine));

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      new SuspiciousExtensionBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);

  // Validate that we don't have a suppress value for the extensions.
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));

  EXPECT_FALSE(controller->ShouldShow());
  std::vector<std::u16string> suspicious_extensions =
      controller->GetExtensionList();
  EXPECT_EQ(0U, suspicious_extensions.size());

  // Now disable an extension, specifying the wipeout flag.
  service_->DisableExtension(kId1, disable_reason::DISABLE_NOT_VERIFIED);

  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new SuspiciousExtensionBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  controller->delegate()->ClearProfileSetForTesting();
  EXPECT_TRUE(controller->ShouldShow());
  suspicious_extensions = controller->GetExtensionList();
  ASSERT_EQ(1U, suspicious_extensions.size());
  EXPECT_EQ(u"Extension 1", suspicious_extensions[0]);
  bubble.set_controller(controller.get());
  bubble.set_action_on_show(FakeExtensionMessageBubble::BUBBLE_ACTION_IGNORE);
  bubble.Show();  // Simulate showing the bubble.

  EXPECT_FALSE(bubble.is_closed());

  // Uninstall the extension.
  service_->UninstallExtension(kId1, UNINSTALL_REASON_FOR_TESTING, nullptr);
  ASSERT_EQ(0U, controller->GetExtensionList().size());

  // The bubble should be closed after the extension is uninstalled.
  EXPECT_TRUE(bubble.is_closed());

  controller.reset();
}

// Tests that a bubble associated with multiple extensions remains shown after
// one of its associated extensions is uninstalled. Also tests that the bubble
// closes when all of its associated extensions are uninstalled.
TEST_F(ExtensionMessageBubbleTest, BubbleShownForMultipleExtensions) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
  Init();
  ASSERT_TRUE(LoadGenericExtension("1", kId1, ManifestLocation::kUnpacked));
  ASSERT_TRUE(LoadGenericExtension("2", kId2, ManifestLocation::kUnpacked));
  ASSERT_TRUE(LoadGenericExtension("3", kId3, ManifestLocation::kUnpacked));

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      new DevModeBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();

  EXPECT_TRUE(controller->ShouldShow());
  ASSERT_EQ(3U, controller->GetExtensionList().size());

  // Simulate showing the bubble and take no action.
  FakeExtensionMessageBubble bubble;
  EXPECT_TRUE(controller->ShouldShow());
  bubble.set_controller(controller.get());
  bubble.set_action_on_show(FakeExtensionMessageBubble::BUBBLE_ACTION_IGNORE);
  bubble.Show();
  EXPECT_FALSE(bubble.is_closed());

  // Uninstall one of the three extensions.
  service_->UninstallExtension(kId1, UNINSTALL_REASON_FOR_TESTING, nullptr);
  ASSERT_EQ(2U, controller->GetExtensionList().size());

  // The bubble should still be shown for the remaining installed extensions.
  EXPECT_FALSE(bubble.is_closed());

  // Uninstall the remaining two extensions.
  service_->UninstallExtension(kId2, UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(kId3, UNINSTALL_REASON_FOR_TESTING, nullptr);
  ASSERT_EQ(0U, controller->GetExtensionList().size());

  // Since all the bubble's associated extensions are uninstalled, the bubble
  // should be closed.
  EXPECT_TRUE(bubble.is_closed());

  controller.reset();
}

void SetInstallTime(const std::string& extension_id,
                    const base::Time& time,
                    ExtensionPrefs* prefs) {
  std::string time_str = base::NumberToString(time.ToInternalValue());
  prefs->UpdateExtensionPref(extension_id, "last_update_time",
                             base::Value(time_str));
}

// The feature this is meant to test is only implemented on Windows and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// http://crbug.com/397426
#define MAYBE_ProxyOverriddenControllerTest DISABLED_ProxyOverriddenControllerTest
#else
#define MAYBE_ProxyOverriddenControllerTest DISABLED_ProxyOverriddenControllerTest
#endif

TEST_F(ExtensionMessageBubbleTest, MAYBE_ProxyOverriddenControllerTest) {
#if BUILDFLAG(IS_MAC)
  // On Mac, this API is limited to trunk.
  ScopedCurrentChannel scoped_channel(version_info::Channel::UNKNOWN);
#endif  // BUILDFLAG(IS_MAC)

  Init();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  // Load two extensions overriding proxy and one overriding something
  // unrelated (to check for interference). Extension 2 should still win
  // on the proxy setting.
  ASSERT_TRUE(
      LoadExtensionOverridingProxy("1", kId1, ManifestLocation::kUnpacked));
  ASSERT_TRUE(
      LoadExtensionOverridingProxy("2", kId2, ManifestLocation::kUnpacked));
  ASSERT_TRUE(
      LoadExtensionOverridingStart("3", kId3, ManifestLocation::kUnpacked));

  // The bubble will not show if the extension was installed in the last 7 days
  // so we artificially set the install time to simulate an old install during
  // testing.
  base::Time old_enough = base::Time::Now() - base::Days(8);
  SetInstallTime(kId1, old_enough, prefs);
  SetInstallTime(kId2, base::Time::Now(), prefs);
  SetInstallTime(kId3, old_enough, prefs);

  std::unique_ptr<TestExtensionMessageBubbleController> controller(
      new TestExtensionMessageBubbleController(
          new ProxyOverriddenBubbleDelegate(browser()->profile()), browser()));
  controller->SetIsActiveBubble();

  // The second extension is too new to warn about.
  EXPECT_FALSE(controller->ShouldShow());
  // Lets make it old enough.
  SetInstallTime(kId2, old_enough, prefs);

  // The list will contain one enabled unpacked extension (ext 2).
  EXPECT_TRUE(controller->ShouldShow());
  EXPECT_FALSE(controller->ShouldShow());
  std::vector<std::u16string> override_extensions =
      controller->GetExtensionList();
  ASSERT_EQ(1U, override_extensions.size());
  EXPECT_EQ(u"Extension 2", override_extensions[0]);
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  EXPECT_EQ(0U, controller->action_click_count());

  // Simulate showing the bubble and dismissing it.
  FakeExtensionMessageBubble bubble;
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_DISMISS_BUTTON);
  bubble.set_controller(controller.get());
  bubble.Show();
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(1U, controller->dismiss_click_count());
  // No extension should have become disabled.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != nullptr);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != nullptr);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != nullptr);
  // Only extension 2 should have been acknowledged.
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_TRUE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId3));
  // Clean up after ourselves.
  controller->delegate()->SetBubbleInfoBeenAcknowledged(kId2, false);

  // Simulate clicking the learn more link to dismiss it.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_LINK);
  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new ProxyOverriddenBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  EXPECT_TRUE(controller->ShouldShow());
  bubble.set_controller(controller.get());
  bubble.Show();
  EXPECT_EQ(1U, controller->link_click_count());
  EXPECT_EQ(0U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  // No extension should have become disabled.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != nullptr);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId2) != nullptr);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != nullptr);
  // Only extension 2 should have been acknowledged.
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_TRUE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId3));
  // Clean up after ourselves.
  controller->delegate()->SetBubbleInfoBeenAcknowledged(kId2, false);

  // Do it again, but now opt to disable the extension.
  bubble.set_action_on_show(
      FakeExtensionMessageBubble::BUBBLE_ACTION_CLICK_ACTION_BUTTON);
  controller = std::make_unique<TestExtensionMessageBubbleController>(
      new ProxyOverriddenBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  EXPECT_TRUE(controller->ShouldShow());
  override_extensions = controller->GetExtensionList();
  EXPECT_EQ(1U, override_extensions.size());
  bubble.set_controller(controller.get());
  bubble.Show();  // Simulate showing the bubble.
  EXPECT_EQ(0U, controller->link_click_count());
  EXPECT_EQ(1U, controller->action_click_count());
  EXPECT_EQ(0U, controller->dismiss_click_count());
  // Only extension 2 should have become disabled.
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId1) != nullptr);
  EXPECT_TRUE(registry->disabled_extensions().GetByID(kId2) != nullptr);
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId3) != nullptr);

  // No extension should have been acknowledged (it got disabled).
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId1));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId2));
  EXPECT_FALSE(controller->delegate()->HasBubbleInfoBeenAcknowledged(kId3));

  // Clean up after ourselves.
  service_->UninstallExtension(kId1, extensions::UNINSTALL_REASON_FOR_TESTING,
                               nullptr);
  service_->UninstallExtension(kId2, extensions::UNINSTALL_REASON_FOR_TESTING,
                               nullptr);
  service_->UninstallExtension(kId3, extensions::UNINSTALL_REASON_FOR_TESTING,
                               nullptr);
}

// Tests that a bubble outliving the associated browser object doesn't crash.
// crbug.com/604003
TEST_F(ExtensionMessageBubbleTest, TestBubbleOutlivesBrowser) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
  Init();
  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(LoadExtensionWithAction("1", kId1, ManifestLocation::kUnpacked));

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      new DevModeBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  EXPECT_TRUE(controller->ShouldShow());
  EXPECT_EQ(1u, model->action_ids().size());
  EXPECT_TRUE(model->has_active_bubble());
  set_browser(nullptr);
  EXPECT_FALSE(model->has_active_bubble());
  controller.reset();
}

// Tests that when an extension -- associated with a bubble controller -- is
// uninstalling after the browser is destroyed, the controller does not access
// the associated browser object and therefore, no use-after-free occurs.
// crbug.com/756316
TEST_F(ExtensionMessageBubbleTest,
       TestUninstallExtensionAfterBrowserDestroyed) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
  Init();
  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(LoadExtensionWithAction("1", kId1, ManifestLocation::kUnpacked));

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      new DevModeBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  EXPECT_TRUE(controller->ShouldShow());
  EXPECT_EQ(1u, model->action_ids().size());
  EXPECT_TRUE(model->has_active_bubble());
  set_browser(nullptr);
  service_->UninstallExtension(kId1, extensions::UNINSTALL_REASON_FOR_TESTING,
                               nullptr);
  EXPECT_FALSE(model->has_active_bubble());
  controller.reset();
}

// Tests that when an extension -- associated with a bubble controller -- is
// disabling after the browser is destroyed, the controller does not access
// the associated browser object and therefore, no use-after-free occurs.
TEST_F(ExtensionMessageBubbleTest,
       TestDisablingExtensionAfterBrowserDestroyed) {
  FeatureSwitch::ScopedOverride force_dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);
  Init();
  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(LoadExtensionWithAction("1", kId1, ManifestLocation::kUnpacked));

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      new DevModeBubbleDelegate(browser()->profile()), browser());
  controller->SetIsActiveBubble();
  EXPECT_TRUE(controller->ShouldShow());
  EXPECT_EQ(1u, model->action_ids().size());
  EXPECT_TRUE(model->has_active_bubble());
  set_browser(nullptr);
  service_->DisableExtension(kId1, disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(model->has_active_bubble());
  controller.reset();
}

// Tests if that ShouldShow() returns false if the bubble's associated extension
// has been removed.
TEST_F(ExtensionMessageBubbleTest,
       ShouldShowReturnsFalseIfExtensionIsDisabled) {
  Init();

  scoped_refptr<const Extension> extension = ExtensionBuilder("Alpha").Build();
  service_->AddExtension(extension.get());
  auto test_delegate = std::make_unique<TestExtensionMessageBubbleDelegate>(
      browser()->profile());
  test_delegate->IncludeExtensionId(extension->id());

  auto controller = std::make_unique<TestExtensionMessageBubbleController>(
      test_delegate.release(), browser());

  ASSERT_EQ(1u, controller->GetExtensionIdList().size());
  EXPECT_EQ(extension->id(), controller->GetExtensionIdList()[0]);
  EXPECT_TRUE(controller->ShouldShow());

  // Disable the extension.
  service_->DisableExtension(extension->id(),
                             extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(controller->ShouldShow());
}

}  // namespace extensions
