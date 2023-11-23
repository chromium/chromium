// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

// Enum used to initialize the parameterized test with different types of
// extensions.
enum class ExtensionType {
  kChromeApp,
  kExtensionMV2,
  kExtensionMV3,
};

// Mapping of the different extension types used in the test to the specific
// manifest file names to create an extension of that type. The actual location
// of these files is at //chrome/test/data/extensions/api_test/document_scan/.
static constexpr auto kManifestFileNames =
    base::MakeFixedFlatMap<ExtensionType, base::StringPiece>(
        {{ExtensionType::kChromeApp, "manifest_chrome_app.json"},
         {ExtensionType::kExtensionMV2, "manifest_extension_v2.json"},
         {ExtensionType::kExtensionMV3, "manifest_extension_v3.json"}});

std::unique_ptr<TestExtensionDir> CreateDocumentScanExtension(
    ExtensionType type) {
  auto extension_dir = std::make_unique<TestExtensionDir>();

  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath document_scan_dir = test_data_dir.AppendASCII("extensions")
                                         .AppendASCII("api_test")
                                         .AppendASCII("document_scan");

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CopyDirectory(document_scan_dir, extension_dir->UnpackedPath(),
                      /*recursive=*/false);
  extension_dir->CopyFileTo(document_scan_dir.AppendASCII(CHECK_DEREF(
                                base::FindOrNull(kManifestFileNames, type))),
                            extensions::kManifestFilename);

  return extension_dir;
}

}  // namespace

class DocumentScanApiTest : public ExtensionApiTest,
                            public testing::WithParamInterface<ExtensionType> {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
  }

 protected:
  ExtensionType GetExtensionType() const { return GetParam(); }

  void RunTest(const char* html_test_page) {
    auto dir = CreateDocumentScanExtension(GetExtensionType());
    auto run_options = GetExtensionType() == ExtensionType::kChromeApp
                           ? RunOptions{.custom_arg = html_test_page,
                                        .launch_as_platform_app = true}
                           : RunOptions({.extension_url = html_test_page});
    ASSERT_TRUE(RunExtensionTest(dir->UnpackedPath(), run_options, {}));
  }
};

IN_PROC_BROWSER_TEST_P(DocumentScanApiTest, TestLoadPermissions) {
  // This test simply checks to see if we have the correct permissions to load
  // the extension.
  RunTest("load_permissions.html");
}

INSTANTIATE_TEST_SUITE_P(/**/,
                         DocumentScanApiTest,
                         testing::Values(ExtensionType::kChromeApp,
                                         ExtensionType::kExtensionMV2,
                                         ExtensionType::kExtensionMV3));
}  // namespace extensions
