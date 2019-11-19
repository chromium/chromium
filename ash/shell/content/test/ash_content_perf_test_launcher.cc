// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include "ash/shell/content/client/shell_main_delegate.h"
#include "ash/test/ui_controls_factory_ash.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/system/sys_info.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"

namespace {

class AshContentTestSuite : public content::ContentTestSuiteBase {
 public:
  AshContentTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {}
  ~AshContentTestSuite() override {}

 protected:
  // content::ContentTestSuiteBase:
  void Initialize() override {
    // Browser tests are expected not to tear-down various globals and may
    // complete with the thread priority being above NORMAL.
    base::TestSuite::DisableCheckForLeakedGlobals();
    base::TestSuite::DisableCheckForThreadPriorityAtTestEnd();
    ContentTestSuiteBase::Initialize();
    ui_controls::InstallUIControlsAura(ash::test::CreateAshUIControls());
  }

  DISALLOW_COPY_AND_ASSIGN(AshContentTestSuite);
};

class AshContentPerfTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  AshContentPerfTestLauncherDelegate() {}
  ~AshContentPerfTestLauncherDelegate() override {}

  // content::TestLancherDelegate:
  int RunTestSuite(int argc, char** argv) override {
    return AshContentTestSuite(argc, argv).Run();
  }
  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override {
    command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                   temp_data_dir);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    return true;
  }

 protected:
  content::ContentMainDelegate* CreateContentMainDelegate() override {
    return new ash::shell::ShellMainDelegate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshContentPerfTestLauncherDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  AshContentPerfTestLauncherDelegate launcher_delegate;
  // Perf tests should run all tests sequentially.
  return LaunchTests(&launcher_delegate, 1, argc, argv);
}
