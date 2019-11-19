// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/extensions/extension_service.h"
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
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using content::NavigationThrottle;

namespace extensions {

namespace {

const char kMatchingUrl[] = "http://google.com/";
const char kNotMatchingUrl[] = "http://example.com/";

// Yoinked from extension_manifest_unittest.cc.
std::unique_ptr<base::DictionaryValue> LoadManifestFile(
    const base::FilePath path,
    std::string* error) {
  EXPECT_TRUE(base::PathExists(path));
  JSONFileValueDeserializer deserializer(path);
  return base::DictionaryValue::From(deserializer.Deserialize(nullptr, error));
}

scoped_refptr<Extension> LoadExtension(const std::string& filename,
                                       std::string* error) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.
      AppendASCII("extensions").
      AppendASCII("manifest_tests").
      AppendASCII(filename.c_str());
  std::unique_ptr<base::DictionaryValue> value = LoadManifestFile(path, error);
  if (!value)
    return nullptr;
  return Extension::Create(path.DirName(), Manifest::UNPACKED, *value,
                           Extension::NO_FLAGS, error);
}

}  // namespace

class UserScriptListenerTest : public testing::Test {
 public:
  UserScriptListenerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        profile_manager_(
            new TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override {
#if defined(OS_CHROMEOS)
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
#endif
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-profile");
    ASSERT_TRUE(profile_);
    TestExtensionSystem* test_extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_));
    service_ = test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    auto instance = content::SiteInstance::Create(profile_);
    instance->GetProcess()->Init();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_, std::move(instance));
  }

  void MarkNavigationResumed() { was_navigation_resumed_ = true; }

 protected:
  void LoadTestExtension() {
    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    base::FilePath extension_path = test_dir
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    UnpackedInstaller::Create(service_)->Load(extension_path);
    content::RunAllTasksUntilIdle();
  }

  void UnloadTestExtension() {
    const extensions::ExtensionSet& extensions =
        ExtensionRegistry::Get(profile_)->enabled_extensions();
    ASSERT_FALSE(extensions.is_empty());
    service_->UnloadExtension((*extensions.begin())->id(),
                              UnloadedExtensionReason::DISABLE);
  }

  std::unique_ptr<NavigationThrottle> CreateListenerNavigationThrottle(
      content::NavigationHandle* handle) {
    std::unique_ptr<NavigationThrottle> throttle =
        listener_.CreateNavigationThrottle(handle);
    throttle->set_resume_callback_for_testing(
        base::BindRepeating(&UserScriptListenerTest::MarkNavigationResumed,
                            base::Unretained(this)));
    return throttle;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  UserScriptListener listener_;
  TestingProfile* profile_ = nullptr;
  ExtensionService* service_ = nullptr;
  bool was_navigation_resumed_ = false;
  std::unique_ptr<content::WebContents> web_contents_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif
};

namespace {

TEST_F(UserScriptListenerTest, DelayAndUpdate) {
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetMainFrame());
  std::unique_ptr<NavigationThrottle> throttle =
      CreateListenerNavigationThrottle(&handle);
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest());

  listener_.TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_TRUE(was_navigation_resumed_);
}

TEST_F(UserScriptListenerTest, DelayAndUnload) {
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetMainFrame());
  std::unique_ptr<NavigationThrottle> throttle =
      CreateListenerNavigationThrottle(&handle);
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest());

  UnloadTestExtension();
  base::RunLoop().RunUntilIdle();

  // This is still not enough to start delayed requests. We have to notify the
  // listener that the user scripts have been updated.
  EXPECT_FALSE(was_navigation_resumed_);

  listener_.TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_TRUE(was_navigation_resumed_);
}

TEST_F(UserScriptListenerTest, NoDelayNoExtension) {
  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetMainFrame());
  std::unique_ptr<NavigationThrottle> throttle =
      listener_.CreateNavigationThrottle(&handle);
  EXPECT_EQ(nullptr, throttle);
}

TEST_F(UserScriptListenerTest, NoDelayNotMatching) {
  LoadTestExtension();

  content::MockNavigationHandle handle(GURL(kNotMatchingUrl),
                                       web_contents_->GetMainFrame());
  std::unique_ptr<NavigationThrottle> throttle =
      listener_.CreateNavigationThrottle(&handle);
  EXPECT_EQ(nullptr, throttle);
}

TEST_F(UserScriptListenerTest, MultiProfile) {
  LoadTestExtension();

  // Fire up a second profile and have it load an extension with a content
  // script.
  TestingProfile* profile2 =
      profile_manager_->CreateTestingProfile("test-profile2");
  ASSERT_TRUE(profile2);
  std::string error;
  scoped_refptr<Extension> extension = LoadExtension(
      "content_script_yahoo.json", &error);
  ASSERT_TRUE(extension.get());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile2);
  registry->AddEnabled(extension);
  registry->TriggerOnLoaded(extension.get());

  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetMainFrame());
  std::unique_ptr<NavigationThrottle> throttle =
      CreateListenerNavigationThrottle(&handle);
  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest());

  // When the first profile's user scripts are ready, the request should still
  // be blocked waiting for profile2.
  listener_.TriggerUserScriptsReadyForTesting(profile_);
  EXPECT_FALSE(was_navigation_resumed_);

  // After profile2 is ready, the request should proceed.
  listener_.TriggerUserScriptsReadyForTesting(profile2);
  EXPECT_TRUE(was_navigation_resumed_);
}

// Test when the user scripts ready trigger occurs before the throttle's
// WillStartRequest function is called.  This can occur when there are multiple
// throttles.
TEST_F(UserScriptListenerTest, ResumeBeforeStart) {
  LoadTestExtension();
  content::MockNavigationHandle handle(GURL(kMatchingUrl),
                                       web_contents_->GetMainFrame());
  std::unique_ptr<NavigationThrottle> throttle =
      listener_.CreateNavigationThrottle(&handle);
  ASSERT_TRUE(throttle);

  listener_.TriggerUserScriptsReadyForTesting(profile_);

  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
}

}  // namespace

}  // namespace extensions
