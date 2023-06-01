// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

class TestingProfile;

namespace base {
class Value;
}

namespace content {
class BrowserTaskEnvironment;
class WebContents;
}

namespace extensions {

class Extension;
class ExtensionPrefs;
class ExtensionService;
class TestExtensionSystem;

// This class provides a minimal environment in which to create
// extensions and tabs for extension-related unittests.
class TestExtensionEnvironment {
 public:
  // Fetches the TestExtensionSystem in |profile| and creates a default
  // ExtensionService there,
  static ExtensionService* CreateExtensionServiceForProfile(
      TestingProfile* profile);

  enum class Type {
    // A TestExtensionEnvironment which will provide a BrowserTaskEnvironment
    // in its scope.
    kWithTaskEnvironment,
    // A TestExtensionEnvironment which will run on top of the existing task
    // environment without trying to provide one.
    kInheritExistingTaskEnvironment,
  };

  explicit TestExtensionEnvironment(Type type = Type::kWithTaskEnvironment);

  TestExtensionEnvironment(const TestExtensionEnvironment&) = delete;
  TestExtensionEnvironment& operator=(const TestExtensionEnvironment&) = delete;

  ~TestExtensionEnvironment();

  TestingProfile* profile() const;

  // Returns the TestExtensionSystem created by the TestingProfile.
  TestExtensionSystem* GetExtensionSystem();

  // Returns an ExtensionService created (and owned) by the
  // TestExtensionSystem created by the TestingProfile.
  ExtensionService* GetExtensionService();

  // Returns ExtensionPrefs created (and owned) by the
  // TestExtensionSystem created by the TestingProfile.
  ExtensionPrefs* GetExtensionPrefs();

  // Creates an Extension and registers it with the ExtensionService.
  // The Extension has a default manifest of {name: "Extension",
  // version: "1.0", manifest_version: 2}, and values in
  // manifest_extra override these defaults.
  const Extension* MakeExtension(const base::Value::Dict& manifest_extra);

  // Use a specific extension ID instead of the default generated in
  // Extension::Create.
  const Extension* MakeExtension(const base::Value::Dict& manifest_extra,
                                 const std::string& id);

  // Generates a valid packaged app manifest with the given ID. If |install|
  // it gets added to the ExtensionService in |profile|.
  scoped_refptr<const Extension> MakePackagedApp(const std::string& id,
                                                 bool install);

  // Returns a test web contents that has a tab id.
  std::unique_ptr<content::WebContents> MakeTab() const;

  // Deletes the testing profile to test profile teardown.
  void DeleteProfile();

 private:
  class ChromeOSEnv;

  void Init();

  // If |task_environment_| is needed, then it needs to constructed before
  // |profile_| and destroyed after |profile_|.
  const std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::unique_ptr<ChromeOSEnv> chromeos_env_;
#endif

#if BUILDFLAG(IS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ExtensionService, DanglingUntriaged> extension_service_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
