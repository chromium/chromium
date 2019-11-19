// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/pepper_flash_component_installer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pepper_flash.h"
#include "content/public/test/browser_task_environment.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
// File name of the Pepper Flash plugin on different platforms.
const base::FilePath::CharType kDataPath[] =
#if defined(OS_MACOSX)
#if defined(ARCH_CPU_X86)
    FILE_PATH_LITERAL("components/flapper/mac");
#elif defined(ARCH_CPU_X86_64)
    FILE_PATH_LITERAL("components/flapper/mac_x64");
#else
    FILE_PATH_LITERAL("components/flapper/NONEXISTENT");
#endif
#elif defined(OS_WIN)
#if defined(ARCH_CPU_X86)
    FILE_PATH_LITERAL("components\\flapper\\windows");
#elif defined(ARCH_CPU_X86_64)
    FILE_PATH_LITERAL("components\\flapper\\windows_x64");
#else
    FILE_PATH_LITERAL("components\\flapper\\NONEXISTENT");
#endif
#elif defined(OS_CHROMEOS)
#if defined(ARCH_CPU_X86)
    FILE_PATH_LITERAL("components/flapper/chromeos_intel32");
#elif defined(ARCH_CPU_X86_64)
    FILE_PATH_LITERAL("components/flapper/chromeos_intel64");
#elif defined(ARCH_CPU_ARMEL)
    FILE_PATH_LITERAL("components/flapper/chromeos_arm32");
#else
    FILE_PATH_LITERAL("components/flapper/NONEXISTENT");
#endif
#else  // OS_LINUX, etc.
#if defined(ARCH_CPU_X86)
    FILE_PATH_LITERAL("components/flapper/linux");
#elif defined(ARCH_CPU_X86_64)
    FILE_PATH_LITERAL("components/flapper/linux_x64");
#else
    FILE_PATH_LITERAL("components/flapper/NONEXISTENT");
#endif
#endif
}  // namespace

// TODO(viettrungluu): Separate out into two separate tests; use a test fixture.
TEST(ComponentInstallerTest, PepperFlashCheck) {
  content::BrowserTaskEnvironment task_environment;

  ppapi::PpapiGlobals::PerThreadForTest per_thread_for_test;
  ppapi::TestGlobals test_globals(per_thread_for_test);
  ppapi::PpapiGlobals::SetPpapiGlobalsOnThreadForTest(&test_globals);

  // The test directory is chrome/test/data/components/flapper.
  base::FilePath manifest;
  base::PathService::Get(chrome::DIR_TEST_DATA, &manifest);
  manifest = manifest.Append(kDataPath);
  manifest = manifest.AppendASCII("manifest.json");

  if (!base::PathExists(manifest)) {
    LOG(WARNING) << "No test manifest available. Skipping.";
    return;
  }

  JSONFileValueDeserializer deserializer(manifest);
  std::string error;

  std::unique_ptr<base::DictionaryValue> root =
      base::DictionaryValue::From(deserializer.Deserialize(NULL, &error));

  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_dict());

  // This checks that the whole manifest is compatible.
  base::Version version;
  EXPECT_TRUE(CheckPepperFlashManifest(*root, &version));
  EXPECT_TRUE(version.IsValid());
}

}  // namespace component_updater
