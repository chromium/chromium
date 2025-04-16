// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_platform_browsertest.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {
class ExtensionBrowserTestPlatformDelegate;
class ExtensionService;

// Base class for extension browser tests. Provides utilities for loading,
// unloading, and installing extensions.
class ExtensionBrowserTest : public ExtensionPlatformBrowserTest {
 public:
  using ContextType = extensions::browser_test_util::ContextType;
  using LoadOptions = extensions::browser_test_util::LoadOptions;

  ExtensionBrowserTest(const ExtensionBrowserTest&) = delete;
  ExtensionBrowserTest& operator=(const ExtensionBrowserTest&) = delete;

 protected:
  // The platform delegate is an implementation detail of the test harness
  // and should be able to access anything any general test would access.
  friend class ExtensionBrowserTestPlatformDelegate;

  explicit ExtensionBrowserTest(ContextType context_type = ContextType::kNone);
  ~ExtensionBrowserTest() override;

  // Useful accessors.
  ExtensionService* extension_service();

  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_check_;

  // Allows MV2 extensions to be loaded.
  std::optional<ScopedTestMV2Enabler> mv2_enabler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSERTEST_H_
