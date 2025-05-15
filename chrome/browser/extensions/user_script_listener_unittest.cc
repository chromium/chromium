// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using content::NavigationThrottle;

namespace extensions {

namespace {

const char kMatchingUrl[] = "http://google.com/";
const char kMatchingPrefsUrl[] = "http://prefs.com/";
const char kNotMatchingUrl[] = "http://example.com/";
const ExtensionId kTestExtensionId = "behllobkkfkfnphdnhnkndlbkcpglgmj";

// Yoinked from manifest_unittest.cc.
std::optional<base::Value::Dict> LoadManifestFile(const base::FilePath path,
                                                  std::string* error) {
  EXPECT_TRUE(base::PathExists(path));
  JSONFileValueDeserializer deserializer(path);
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(nullptr, error);
  if (!manifest || !manifest->is_dict()) {
    return std::nullopt;
  }
  return std::move(*manifest).TakeDict();
}

scoped_refptr<Extension> LoadExtension(const std::string& filename,
                                       std::string* error) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII("manifest_tests")
             .AppendASCII(filename.c_str());
  std::optional<base::Value::Dict> manifest = LoadManifestFile(path, error);
  if (!manifest) {
    return nullptr;
  }
  return Extension::Create(path.DirName(), mojom::ManifestLocation::kUnpacked,
                           *manifest, Extension::NO_FLAGS, error);
}

}  // namespace

class UserScriptListenerTest : public testing::Test {
 public:
  UserScriptListenerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        profile_manager_(
            new TestingProfileManager(TestingBrowserProcess::GetGlobal())) {
    // Allow unpacked extensions without developer mode for testing.
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionDisableUnsupportedDeveloper);
  }

  ~UserScriptListenerTest() override = default;

  UserScriptListenerTest(const UserScriptListenerTest&) = delete;
  UserScriptListenerTest& operator=(const UserScriptListenerTest&) = delete;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
#endif
    ASSERT_TRUE(profile_manager_->SetUp());
    // The listener must be set up after the profile manager has been set up/
    // installed itself on the browser process.
    listener_ = std::make_unique<UserScriptListener>();
    profile_ = profile_manager_->CreateTestingProfile("test-profile");
    ASSERT_TRUE(profile_);
    TestExtensionSystem* test_extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_));
    test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    auto instance = content::SiteInstance::Create(profile_);
    instance->GetOrCreateProcess()->Init();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_, std::move(instance));
  }

  void TearDown() override {
    // The Listener unsubscribes itself from the profile in StartTearDown;
    // failure to unsubscribe will result in the profile_manager's destructor
    // throwing an error since there's still a subscription in the callback
    // list.
    listener_->StartTearDown();
  }

  void MarkNavigationResumed() { was_navigation_resumed_ = true; }

 protected:
  void LoadTestExtension() {
    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    base::FilePath extension_path = test_dir.AppendASCII("extensions")
                                        .AppendASCII("good")
                                        .AppendASCII("Extensions")
                                        .AppendASCII(kTestExtensionId)
                                        .AppendASCII("1.0.0.0");
    TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile_),
                                           kTestExtensionId);
    UnpackedInstaller::Create(profile_)->Load(extension_path);
    observer.WaitForExtensionLoaded();
  }

  void UnloadTestExtension() {
    const ExtensionSet& extensions =
        ExtensionRegistry::Get(profile_)->enabled_extensions();
    ASSERT_FALSE(extensions.empty());
    ExtensionRegistrar::Get(profile_)->RemoveExtension(
        (*extensions.begin())->id(), UnloadedExtensionReason::DISABLE);
  }

  void CreateListenerNavigationThrottle(
      content::MockNavigationThrottleRegistry& registry) {
    ASSERT_TRUE(registry.throttles().empty());
    listener_->CreateAndAddNavigationThrottle(registry);
    ASSERT_EQ(registry.throttles().size(), 1u);
    registry.throttles().back()->set_resume_callback_for_testing(
        base::BindRepeating(&UserScriptListenerTest::MarkNavigationResumed,
                            base::Unretained(this)));
  }

  void AddPersistentScriptingURLPatternToPrefs() {
    URLPatternSet persistent_urls;
    persistent_urls.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, kMatchingPrefsUrl));
    scripting::SetPersistentScriptURLPatterns(profile_, kTestExtensionId,
                                              persistent_urls);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<UserScriptListener> listener_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  bool was_navigation_resumed_ = false;
  std::unique_ptr<content::WebContents> web_contents_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif
};

namespace {

TEST_F(UserScriptListenerTest, DelayAndUpdate) {
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateListenerNavigationThrottle(registry);
  EXPECT_EQ(NavigationThrottle::DEFER,
            registry.throttles().back()->WillStartRequest());

  listener_->TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_TRUE(was_navigation_resumed_);
}

// Test that requests matching URL patterns from persistent dynamic content
// scripts registered from previous sessions (stored inside prefs) are
// throttled.
TEST_F(UserScriptListenerTest, DelayForPersistentScriptPatterns) {
  AddPersistentScriptingURLPatternToPrefs();
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kMatchingPrefsUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateListenerNavigationThrottle(registry);
  EXPECT_EQ(NavigationThrottle::DEFER,
            registry.throttles().back()->WillStartRequest());

  listener_->TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_TRUE(was_navigation_resumed_);
}

TEST_F(UserScriptListenerTest, DelayAndUnload) {
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateListenerNavigationThrottle(registry);
  EXPECT_EQ(NavigationThrottle::DEFER,
            registry.throttles().back()->WillStartRequest());

  UnloadTestExtension();
  base::RunLoop().RunUntilIdle();

  // This is still not enough to start delayed requests. We have to notify the
  // listener that the user scripts have been updated.
  EXPECT_FALSE(was_navigation_resumed_);

  listener_->TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_TRUE(was_navigation_resumed_);
}

TEST_F(UserScriptListenerTest, NoDelayNoExtension) {
  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  listener_->CreateAndAddNavigationThrottle(registry);
  EXPECT_EQ(registry.throttles().size(), 0u);
}

TEST_F(UserScriptListenerTest, NoDelayNotMatching) {
  AddPersistentScriptingURLPatternToPrefs();
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kNotMatchingUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  listener_->CreateAndAddNavigationThrottle(registry);
  EXPECT_EQ(registry.throttles().size(), 0u);
}

TEST_F(UserScriptListenerTest, MultiProfile) {
  LoadTestExtension();

  // Fire up a second profile and have it load an extension with a content
  // script.
  TestingProfile* profile2 =
      profile_manager_->CreateTestingProfile("test-profile2");
  ASSERT_TRUE(profile2);
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension("content_script_yahoo.json", &error);
  ASSERT_TRUE(extension.get());

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile2);
  extension_registry->AddEnabled(extension);
  extension_registry->TriggerOnLoaded(extension.get());

  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry throttle_registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateListenerNavigationThrottle(throttle_registry);
  EXPECT_EQ(NavigationThrottle::DEFER,
            throttle_registry.throttles().back()->WillStartRequest());

  // When the first profile's user scripts are ready, the request should still
  // be blocked waiting for profile2.
  listener_->TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_FALSE(was_navigation_resumed_);

  // After profile2 is ready, the request should proceed.
  listener_->TriggerUserScriptsReadyForTesting(profile2);
  EXPECT_TRUE(was_navigation_resumed_);
}

// Test when the user scripts ready trigger occurs before the throttle's
// WillStartRequest function is called.  This can occur when there are multiple
// throttles.
TEST_F(UserScriptListenerTest, ResumeBeforeStart) {
  LoadTestExtension();
  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  listener_->CreateAndAddNavigationThrottle(registry);
  CHECK_EQ(registry.throttles().size(), 1u);
  auto throttle = std::move(registry.throttles().back());

  listener_->TriggerUserScriptsReadyForTesting(profile_);

  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
}

}  // namespace

}  // namespace extensions
