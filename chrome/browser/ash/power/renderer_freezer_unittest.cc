// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/renderer_freezer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
// Class that delegates used in testing can inherit from to record calls that
// are made by the code being tested.
class ActionRecorder {
 public:
  ActionRecorder() {}

  ActionRecorder(const ActionRecorder&) = delete;
  ActionRecorder& operator=(const ActionRecorder&) = delete;

  virtual ~ActionRecorder() {}

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActions() (i.e. results are
  // non-repeatable).
  std::string GetActions() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

 protected:
  // Appends |new_action| to |actions_|, using a comma as a separator if
  // other actions are already listed.
  void AppendAction(const std::string& new_action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += new_action;
  }

 private:
  // Comma-separated list of actions that have been performed.
  std::string actions_;
};

// Actions that can be returned by TestDelegate::GetActions().
const char kSetShouldFreezeRenderer[] = "set_should_freeze_renderer";
const char kSetShouldNotFreezeRenderer[] = "set_should_not_freeze_renderer";
const char kFreezeRenderers[] = "freeze_renderers";
const char kThawRenderers[] = "thaw_renderers";
const char kNoActions[] = "";

// Test implementation of RendererFreezer::Delegate that records the actions it
// was asked to perform.
class TestDelegate : public RendererFreezer::Delegate, public ActionRecorder {
 public:
  TestDelegate()
      : can_freeze_renderers_(true),
        thaw_renderers_result_(true) {}

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  ~TestDelegate() override {}

  // RendererFreezer::Delegate overrides.
  void SetShouldFreezeRenderer(base::ProcessHandle handle,
                               bool frozen) override {
    AppendAction(frozen ? kSetShouldFreezeRenderer
                        : kSetShouldNotFreezeRenderer);
  }
  void FreezeRenderers() override {
    AppendAction(kFreezeRenderers);
  }
  void ThawRenderers(ResultCallback callback) override {
    AppendAction(kThawRenderers);

    std::move(callback).Run(thaw_renderers_result_);
  }
  void CheckCanFreezeRenderers(ResultCallback callback) override {
    std::move(callback).Run(can_freeze_renderers_);
  }

  void set_thaw_renderers_result(bool result) {
    thaw_renderers_result_ = result;
  }

  // Sets whether the delegate is capable of freezing renderers.  This also
  // changes |freeze_renderers_result_| and |thaw_renderers_result_|.
  void set_can_freeze_renderers(bool can_freeze) {
    can_freeze_renderers_ = can_freeze;

    thaw_renderers_result_ = can_freeze;
  }

 private:
  bool can_freeze_renderers_;
  bool thaw_renderers_result_;
};

}  // namespace

class RendererFreezerTest : public testing::Test {
 public:
  RendererFreezerTest() : test_delegate_(new TestDelegate()) {}

  RendererFreezerTest(const RendererFreezerTest&) = delete;
  RendererFreezerTest& operator=(const RendererFreezerTest&) = delete;

  ~RendererFreezerTest() override = default;

  // testing::Test:
  void SetUp() override { chromeos::PowerManagerClient::InitializeFake(); }

  void TearDown() override {
    DCHECK(renderer_freezer_);
    chromeos::PowerManagerClient::Shutdown();
    renderer_freezer_.reset();
  }

  void SimulateRenderProcessHostCreated(content::RenderProcessHost* rph) {
    renderer_freezer_->OnRenderProcessHostCreated(rph);
  }

 protected:
  void Init() {
    renderer_freezer_ = std::make_unique<RendererFreezer>(
        std::unique_ptr<RendererFreezer::Delegate>(test_delegate_));
  }

  // Owned by |renderer_freezer_|.
  raw_ptr<TestDelegate, DanglingUntriaged> test_delegate_;
  std::unique_ptr<RendererFreezer> renderer_freezer_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the RendererFreezer freezes renderers on suspend and thaws them on
// resume.
TEST_F(RendererFreezerTest, SuspendResume) {
  Init();

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(kFreezeRenderers, test_delegate_->GetActions());

  // The renderers should be thawed when we resume.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_EQ(kThawRenderers, test_delegate_->GetActions());
}

// Tests that the renderer freezer does nothing if the delegate cannot freeze
// renderers.
TEST_F(RendererFreezerTest, DelegateCannotFreezeRenderers) {
  test_delegate_->set_can_freeze_renderers(false);
  Init();

  // Nothing happens on suspend.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());

  // Nothing happens on resume.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_EQ(kNoActions, test_delegate_->GetActions());
}

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that the RendererFreezer crashes the browser if the freezing operation
// was successful but the thawing operation failed.
TEST_F(RendererFreezerTest, ErrorThawingRenderers) {
  // The "threadsafe" style of death test re-executes the unit test binary,
  // which in turn re-initializes some global state leading to failed CHECKs.
  // Instead, we use the "fast" style here to prevent re-initialization.
  GTEST_FLAG_SET(death_test_style, "fast");
  Init();
  test_delegate_->set_thaw_renderers_result(false);

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(kFreezeRenderers, test_delegate_->GetActions());

  EXPECT_DEATH(chromeos::FakePowerManagerClient::Get()->SendSuspendDone(),
               "Unable to thaw");
}
#endif  // GTEST_HAS_DEATH_TEST

class RendererFreezerTestWithExtensions : public RendererFreezerTest {
 public:
  RendererFreezerTestWithExtensions() {}

  RendererFreezerTestWithExtensions(const RendererFreezerTestWithExtensions&) =
      delete;
  RendererFreezerTestWithExtensions& operator=(
      const RendererFreezerTestWithExtensions&) = delete;

  ~RendererFreezerTestWithExtensions() override {}

  // testing::Test overrides.
  void SetUp() override {
    RendererFreezerTest::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());

    // Must be called from testing::Test::SetUp.
    EXPECT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile("RendererFreezerTest");

    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled*/);
  }
  void TearDown() override {
    extensions::ExtensionSystem::Get(profile_)->Shutdown();

    profile_ = nullptr;

    profile_manager_->DeleteAllTestingProfiles();

    base::RunLoop().RunUntilIdle();

    profile_manager_.reset();

    RendererFreezerTest::TearDown();
  }

 protected:
  void CreateRenderProcessForExtension(const extensions::Extension* extension) {
    std::unique_ptr<content::MockRenderProcessHostFactory> rph_factory(
        new content::MockRenderProcessHostFactory());
    scoped_refptr<content::SiteInstance> site_instance(
        extensions::ProcessManager::Get(profile_)->GetSiteInstanceForURL(
            extensions::BackgroundInfo::GetBackgroundURL(extension)));
    content::RenderProcessHost* rph =
        rph_factory->CreateRenderProcessHost(profile_, site_instance.get());

    // Fake that the RenderProcessHost is hosting the gcm app.
    extensions::ProcessMap::Get(profile_)->Insert(extension->id(),
                                                  rph->GetID());

    SimulateRenderProcessHostCreated(rph);
  }

  // Owned by |profile_manager_|.
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  // Chrome OS needs the CrosSettings test helper.
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

// Tests that the RendererFreezer freezes renderers that are not hosting
// GCM extensions.
TEST_F(RendererFreezerTestWithExtensions, FreezesNonExtensionRenderers) {
  Init();

  // Create the mock RenderProcessHost.
  std::unique_ptr<content::MockRenderProcessHostFactory> rph_factory(
      new content::MockRenderProcessHostFactory());
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(profile_));
  content::RenderProcessHost* rph =
      rph_factory->CreateRenderProcessHost(profile_, site_instance.get());

  SimulateRenderProcessHostCreated(rph);

  EXPECT_EQ(kSetShouldFreezeRenderer, test_delegate_->GetActions());
}

// Tests that the RendererFreezer does not freeze renderers that are hosting
// extensions that use GCM.
TEST_F(RendererFreezerTestWithExtensions, DoesNotFreezeGcmExtensionRenderers) {
  Init();

  // First build the GCM extension.
  scoped_refptr<const extensions::Extension> gcm_app =
      extensions::ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "GCM App")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js"))))
                  .Set("permissions", base::Value::List().Append("gcm")))
          .Build();

  // Now install it and give it a renderer.
  extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->AddExtension(gcm_app.get());
  CreateRenderProcessForExtension(gcm_app.get());

  EXPECT_EQ(kSetShouldNotFreezeRenderer, test_delegate_->GetActions());
}

// Tests that the RendererFreezer freezes renderers that are hosting extensions
// that do not use GCM.
TEST_F(RendererFreezerTestWithExtensions, FreezesNonGcmExtensionRenderers) {
  Init();

  // First build the extension.
  scoped_refptr<const extensions::Extension> background_app =
      extensions::ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "Background App")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js")))))
          .Build();

  // Now install it and give it a renderer.
  extensions::ExtensionSystem::Get(profile_)
      ->extension_service()
      ->AddExtension(background_app.get());
  CreateRenderProcessForExtension(background_app.get());

  EXPECT_EQ(kSetShouldFreezeRenderer, test_delegate_->GetActions());
}

}  // namespace ash
