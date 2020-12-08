// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

void GetPluginsInfoCallback(
    std::vector<content::WebPluginInfo>* rv,
    base::OnceClosure quit_task,
    const std::vector<content::WebPluginInfo>& plugins) {
  *rv = plugins;
  std::move(quit_task).Run();
}

std::vector<content::WebPluginInfo> GetPlugins() {
  std::vector<content::WebPluginInfo> plugins;
  base::RunLoop run_loop;
  auto callback =
      base::BindOnce(&GetPluginsInfoCallback, &plugins, run_loop.QuitClosure());
  content::PluginService::GetInstance()->GetPlugins(std::move(callback));
  run_loop.RunUntilIdle();
  return plugins;
}

}  // namespace

using ChromePluginTest = InProcessBrowserTest;

// Verify that the official builds have the known set of plugins.
#if BUILDFLAG(IS_CHROMEOS_ASH)  // http://crbug.com/1147726
#define MAYBE_InstalledPlugins DISABLED_InstalledPlugins
#else
#define MAYBE_InstalledPlugins InstalledPlugins
#endif
IN_PROC_BROWSER_TEST_F(ChromePluginTest, MAYBE_InstalledPlugins) {
  const char* expected[] = {
      "Chrome PDF Plugin",
      "Native Client",
  };

  std::vector<content::WebPluginInfo> plugins = GetPlugins();
  for (size_t i = 0; i < base::size(expected); ++i) {
    size_t j = 0;
    for (; j < plugins.size(); ++j) {
      if (plugins[j].name == base::ASCIIToUTF16(expected[i]))
        break;
    }
    ASSERT_TRUE(j != plugins.size()) << "Didn't find " << expected[i];
  }
}
