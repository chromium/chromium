// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/component_manager.h"

#include <string_view>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {

namespace {

// The fake path for the update check.
constexpr std::string_view kFakeUpdateCheckPath = "/fake_update_check";

}  // namespace

// Tests for ComponentManager.
class ComponentManagerUpdateCheckBrowserTest : public InProcessBrowserTest {
 public:
  ComponentManagerUpdateCheckBrowserTest() = default;
  ~ComponentManagerUpdateCheckBrowserTest() override = default;

  // Disallow copy and assign.
  ComponentManagerUpdateCheckBrowserTest(
      const ComponentManagerUpdateCheckBrowserTest&) = delete;
  ComponentManagerUpdateCheckBrowserTest& operator=(
      const ComponentManagerUpdateCheckBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // Registers a fake component-updater check handler.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ComponentManagerUpdateCheckBrowserTest::RequestHandler,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set the component-updater check URL to the fake one.
    command_line->AppendSwitchASCII(
        "component-updater",
        base::StrCat(
            {"url-source=",
             embedded_test_server()->GetURL(kFakeUpdateCheckPath).spec()}));
  }

 protected:
  // Sets the callback to be called when an update check is requested.
  void SetOnUpdateCheckRequestedCallback(
      base::OnceClosure on_update_check_requested_callback) {
    on_update_check_requested_callback_ =
        std::move(on_update_check_requested_callback);
  }

 private:
  // A fake update check handler that calls the callback when an update check is
  // requested.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.starts_with(kFakeUpdateCheckPath) &&
        on_update_check_requested_callback_) {
      std::move(on_update_check_requested_callback_).Run();
    }
    return nullptr;
  }

  base::OnceClosure on_update_check_requested_callback_;
};

// Tests that the translate kit component can be registered only once.
IN_PROC_BROWSER_TEST_F(ComponentManagerUpdateCheckBrowserTest,
                       RegisterTranslateKitComponent) {
  base::RunLoop run_loop;
  SetOnUpdateCheckRequestedCallback(run_loop.QuitClosure());
  EXPECT_TRUE(ComponentManager::GetInstance().RegisterTranslateKitComponent());
  // Wait for the update check is requested.
  run_loop.Run();
  EXPECT_FALSE(ComponentManager::GetInstance().RegisterTranslateKitComponent());
}

// Tests that the translate kit language pack component can be registered and
// unregistered.
IN_PROC_BROWSER_TEST_F(ComponentManagerUpdateCheckBrowserTest,
                       RegisterAndUnregisterTranslateKitLanguagePackComponent) {
  base::RunLoop run_loop;
  SetOnUpdateCheckRequestedCallback(run_loop.QuitClosure());
  ComponentManager::GetInstance().RegisterTranslateKitLanguagePackComponent(
      LanguagePackKey::kEn_Ja);
  // Wait for the update check is requested.
  run_loop.Run();
  EXPECT_TRUE(
      g_browser_process->local_state()->GetBoolean(GetRegisteredFlagPrefName(
          *kLanguagePackComponentConfigMap.at(LanguagePackKey::kEn_Ja))));
  ComponentManager::GetInstance().UninstallTranslateKitLanguagePackComponent(
      LanguagePackKey::kEn_Ja);
  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(GetRegisteredFlagPrefName(
          *kLanguagePackComponentConfigMap.at(LanguagePackKey::kEn_Ja))));
}

using ComponentManagerBrowserTest = InProcessBrowserTest;

// Tests that the translate kit component path is returned correctly.
IN_PROC_BROWSER_TEST_F(ComponentManagerBrowserTest,
                       GetTranslateKitComponentPath) {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);
  EXPECT_EQ(ComponentManager::GetInstance().GetTranslateKitComponentPath(),
            components_dir.Append(kTranslateKitBinaryInstallationRelativeDir));
}

class ComponentManagerCustomComponentPathBrowserTest
    : public InProcessBrowserTest {
 public:
  ComponentManagerCustomComponentPathBrowserTest() {
    CHECK(tmp_dir_.CreateUniqueTempDir());
  }
  ~ComponentManagerCustomComponentPathBrowserTest() override = default;

  // Disallow copy and assign.
  ComponentManagerCustomComponentPathBrowserTest(
      const ComponentManagerCustomComponentPathBrowserTest&) = delete;
  ComponentManagerCustomComponentPathBrowserTest& operator=(
      const ComponentManagerCustomComponentPathBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchPath("translate-kit-binary-path",
                                   GetTempDir().AppendASCII("fake.so"));
  }

 protected:
  const base::FilePath& GetTempDir() { return tmp_dir_.GetPath(); }

 private:
  base::ScopedTempDir tmp_dir_;
};

// Tests that the translate kit component path is returned correctly when the
// path is set by the command line flag.
IN_PROC_BROWSER_TEST_F(ComponentManagerCustomComponentPathBrowserTest,
                       GetTranslateKitComponentPath) {
  EXPECT_EQ(ComponentManager::GetInstance().GetTranslateKitComponentPath(),
            GetTempDir().AppendASCII("fake.so"));
}

}  // namespace on_device_translation
