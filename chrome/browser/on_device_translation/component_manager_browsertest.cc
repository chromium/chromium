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
#include "chrome/test/base/in_process_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {

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

  void InitFakeInstaller() {
    installer_ = std::make_unique<FakeOnDeviceTranslationInstaller>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  std::unique_ptr<FakeOnDeviceTranslationInstaller> installer_;
};

// Tests that the translate kit component can be registered only once.
IN_PROC_BROWSER_TEST_F(ComponentManagerUpdateCheckBrowserTest,
                       RegisterTranslateKitComponent) {
  InitFakeInstaller();
  EXPECT_TRUE(ComponentManager::GetInstance().RegisterTranslateKitComponent());
  // Wait for the update check is requested.
  EXPECT_FALSE(ComponentManager::GetInstance().RegisterTranslateKitComponent());
}

class TestObserver : public OnDeviceTranslationInstaller::Observer {
 public:
  // We pass a callback (the QuitClosure) to the observer.
  explicit TestObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnLanguagePackInstalled(const LanguagePackKey lang_pack) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

// Tests that the translate kit language pack component can be registered and
// unregistered.
IN_PROC_BROWSER_TEST_F(ComponentManagerUpdateCheckBrowserTest,
                       RegisterAndUnregisterTranslateKitLanguagePackComponent) {
  InitFakeInstaller();
  ComponentManager::GetInstance().RegisterTranslateKitComponent();
  base::RunLoop lpack_run_loop;
  TestObserver observer(lpack_run_loop.QuitClosure());
  OnDeviceTranslationInstaller::GetInstance()->AddOserver(&observer);
  ComponentManager::GetInstance().RegisterTranslateKitLanguagePackComponent(
      LanguagePackKey::kEn_Ja);
  lpack_run_loop.Run();

  EXPECT_THAT(ComponentManager::GetInstance().GetRegisteredLanguagePacks(),
              ::testing::UnorderedElementsAre(LanguagePackKey::kEn_Ja));

  ComponentManager::GetInstance().UninstallTranslateKitLanguagePackComponent(
      LanguagePackKey::kEn_Ja);
  EXPECT_THAT(ComponentManager::GetInstance().GetRegisteredLanguagePacks(),
              ::testing::IsEmpty());
}

using ComponentManagerBrowserTest = InProcessBrowserTest;

// Tests that the translate kit component path is returned correctly.
IN_PROC_BROWSER_TEST_F(ComponentManagerBrowserTest,
                       GetTranslateKitComponentPath) {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);
  EXPECT_EQ(ComponentManager::GetInstance().GetTranslateKitComponentPath(),
            components_dir.Append(GetBinaryRelativeInstallDir()));
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
