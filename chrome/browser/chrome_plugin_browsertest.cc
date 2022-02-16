// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/nacl/common/buildflags.h"
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
  run_loop.Run();
  return plugins;
}

}  // namespace

using ChromePluginTest = InProcessBrowserTest;

// Verify a known subset of plugins for the build configuration.
// TODO(https://crbug.com/1297566): Fix and re-eanble test.
IN_PROC_BROWSER_TEST_F(ChromePluginTest, DISABLED_InstalledPlugins) {
  base::flat_set<std::string> expected = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "Chrome PDF Plugin",

#if BUILDFLAG(ENABLE_NACL)
    "Native Client",
#endif  // BUILDFLAG(ENABLE_NACL)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };

  auto actual = GetPlugins();
  for (const auto& cur_actual : actual) {
    expected.erase(base::UTF16ToASCII(cur_actual.name));
  }

  for (const auto& not_found : expected) {
    ADD_FAILURE() << "Didn't find " << not_found;
  }
}
