// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/file_manager/grit/file_manager_resources.h"
#endif

namespace extensions {

class ChromeComponentExtensionResourceManagerTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests IsComponentExtensionResource function.
TEST_F(ChromeComponentExtensionResourceManagerTest,
       IsComponentExtensionResource) {
  const ComponentExtensionResourceManager* resource_manager =
      ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager();
  ASSERT_TRUE(resource_manager);

  // Get the extension test data path.
  base::FilePath test_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_path));
  test_path = test_path.AppendASCII("extensions").AppendASCII("file_manager");

  // Load the manifest data.
  std::string error;
  std::optional<base::Value::Dict> manifest(file_util::LoadManifest(
      test_path, FILE_PATH_LITERAL("app.json"), &error));
  ASSERT_TRUE(manifest) << error;

  // Build a path inside Chrome's resources directory where a component
  // extension might be installed.
  base::FilePath resources_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES, &resources_path));
  resources_path = resources_path.AppendASCII("file_manager");

  // Create a simulated component extension.
  scoped_refptr<Extension> extension =
      Extension::Create(resources_path, mojom::ManifestLocation::kComponent,
                        *manifest, Extension::NO_FLAGS, &error);
  ASSERT_TRUE(extension.get());

  // Load one of the icons.
  ExtensionResource resource = IconsInfo::GetIconResource(
      extension.get(), extension_misc::EXTENSION_ICON_BITTY,
      ExtensionIconSet::Match::kExactly);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The resource is a component resource.
  int resource_id = 0;
  ASSERT_TRUE(resource_manager->IsComponentExtensionResource(
      extension->path(), resource.relative_path(), &resource_id));
  ASSERT_EQ(IDR_FILE_MANAGER_ICON_16, resource_id);
#endif
}

}  // namespace extensions
