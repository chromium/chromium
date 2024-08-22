// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_reinstaller.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

constexpr char kWebstoreDomain[] = "cws.com";
constexpr char kAppDomain[] = "app.com";
constexpr char kNonAppDomain[] = "nonapp.com";
constexpr char kTestExtensionId[] = "ecglahbcnmdpdciemllbhojghbkagdje";
constexpr char kTestDataPath[] = "extensions/api_test/webstore_inline_install";
constexpr char kCrxFilename[] = "extension.crx";

// The values of the mock protobuf response should match those in the JSON API
// that's stored in "chrome/test/data/extensions/api_test/" +
// "webstore_inline_install/inlineinstall/detail/" +
// "ecglahbcnmdpdciemllbhojghbkagdje".
constexpr char kMockTitle[] = "Inline Install Test Extension";
constexpr char kMockUserCountString[] = "371,674";
constexpr double kMockAverageRating = 4.36;
constexpr int kMockRatingCount = 788;
constexpr char kMockRatingCountString[] = "788";
constexpr char kMockLogoUri[] = "webstore_inline_install/extension/icon.png";
constexpr char kMockManifest[] = R"({
  "name": "Inline Install Test Extension",
  "version": "0.1",
  "manifest_version": 2,
  "icons": {"128": "icon.png"},
  "permissions":["tabs"]
})";

std::unique_ptr<FetchItemSnippetResponse> CreateMockResponse(
    const ExtensionId& id) {
  auto mock_response = std::make_unique<FetchItemSnippetResponse>();
  mock_response->set_item_id(id);
  mock_response->set_title(kMockTitle);
  mock_response->set_manifest(kMockManifest);
  mock_response->set_logo_uri(kMockLogoUri);
  mock_response->set_user_count_string(kMockUserCountString);
  mock_response->set_rating_count_string(kMockRatingCountString);
  mock_response->set_rating_count(kMockRatingCount);
  mock_response->set_average_rating(kMockAverageRating);

  return mock_response;
}

}  // namespace

class WebstoreReinstallerBrowserTest : public WebstoreInstallerTest {
 public:
  WebstoreReinstallerBrowserTest()
      : WebstoreInstallerTest(kWebstoreDomain,
                              kTestDataPath,
                              kCrxFilename,
                              kAppDomain,
                              kNonAppDomain) {
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kUseItemSnippetsAPI);
  }
  ~WebstoreReinstallerBrowserTest() override {}

  void OnInstallCompletion(base::OnceClosure quit_closure,
                           bool success,
                           const std::string& error,
                           webstore_install::Result result);

  bool last_install_result() const { return last_install_result_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool last_install_result_;
};

void WebstoreReinstallerBrowserTest::OnInstallCompletion(
    base::OnceClosure quit_closure,
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  last_install_result_ = success;
  std::move(quit_closure).Run();
}

// TODO(crbug.com/325314721): Remove this test once we stop using the old item
// JSON API to fetch webstore data.
IN_PROC_BROWSER_TEST_F(WebstoreReinstallerBrowserTest, TestWebstoreReinstall) {
  // Build an extension with the same id as our test extension and add it.
  const std::string kExtensionName("ReinstallerExtension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(kTestExtensionId)
          .SetManifest(
              base::Value::Dict()
                  .Set("name", kExtensionName)
                  .Set("description", "Foo")
                  .Set("manifest_version", 2)
                  .Set("version", "1.0")
                  .Set("update_url",
                       "https://clients2.google.com/service/update2/crx"))
          .Build();
  extension_service()->AddExtension(extension.get());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));

  // WebstoreReinstaller expects corrupted extension.
  extension_service()->DisableExtension(kTestExtensionId,
                                        disable_reason::DISABLE_CORRUPTED);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Start by canceling the repair prompt.
  AutoCancelInstall();

  // Create and run a WebstoreReinstaller.
  base::RunLoop run_loop;
  auto reinstaller = base::MakeRefCounted<WebstoreReinstaller>(
      active_web_contents, kTestExtensionId,
      base::BindOnce(&WebstoreReinstallerBrowserTest::OnInstallCompletion,
                     base::Unretained(this), run_loop.QuitClosure()));
  reinstaller->BeginReinstall();
  run_loop.Run();

  // We should have failed, and the old extension should still be present.
  EXPECT_FALSE(last_install_result());
  extension = registry->disabled_extensions().GetByID(kTestExtensionId);
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(kExtensionName, extension->name());

  // Now accept the repair prompt.
  AutoAcceptInstall();
  base::RunLoop run_loop2;
  reinstaller = base::MakeRefCounted<WebstoreReinstaller>(
      active_web_contents, kTestExtensionId,
      base::BindOnce(&WebstoreReinstallerBrowserTest::OnInstallCompletion,
                     base::Unretained(this), run_loop2.QuitClosure()));
  reinstaller->BeginReinstall();
  run_loop2.Run();

  // The reinstall should have succeeded, and the extension should have been
  // "updated" (which in this case means that it should have been replaced with
  // the inline install test extension, since that's the id we used).
  EXPECT_TRUE(last_install_result());
  extension = registry->enabled_extensions().GetByID(kTestExtensionId);
  ASSERT_TRUE(extension.get());
  // The name should not match, since the extension changed.
  EXPECT_NE(kExtensionName, extension->name());
}

class WebstoreReinstallerItemSnippetsBrowserTest
    : public WebstoreReinstallerBrowserTest {
 public:
  WebstoreReinstallerItemSnippetsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kUseItemSnippetsAPI);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A version of the WebstoreReinstallerBrowserTest test with the same name
// except this tests that reinstalls for corrupted extensions work when using
// the new item snippets API which returns a protobuf object for web store data
// (the API return is mocked for this test).
IN_PROC_BROWSER_TEST_F(WebstoreReinstallerItemSnippetsBrowserTest,
                       TestWebstoreReinstall) {
  // Build an extension with the same id as our test extension and add it.
  const std::string kExtensionName("ReinstallerExtension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(kTestExtensionId)
          .SetManifest(
              base::Value::Dict()
                  .Set("name", kExtensionName)
                  .Set("description", "Foo")
                  .Set("manifest_version", 2)
                  .Set("version", "1.0")
                  .Set("update_url",
                       "https://clients2.google.com/service/update2/crx"))
          .Build();
  extension_service()->AddExtension(extension.get());

  auto mock_response = CreateMockResponse(kTestExtensionId);
  WebstoreDataFetcher::SetMockItemSnippetReponseForTesting(mock_response.get());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));

  // WebstoreReinstaller expects corrupted extension.
  extension_service()->DisableExtension(kTestExtensionId,
                                        disable_reason::DISABLE_CORRUPTED);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Start by canceling the repair prompt.
  AutoCancelInstall();

  // Create and run a WebstoreReinstaller.
  base::RunLoop run_loop;
  auto reinstaller = base::MakeRefCounted<WebstoreReinstaller>(
      active_web_contents, kTestExtensionId,
      base::BindOnce(&WebstoreReinstallerBrowserTest::OnInstallCompletion,
                     base::Unretained(this), run_loop.QuitClosure()));
  reinstaller->BeginReinstall();
  run_loop.Run();

  // We should have failed, and the old extension should still be present.
  EXPECT_FALSE(last_install_result());
  extension = registry->disabled_extensions().GetByID(kTestExtensionId);
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(kExtensionName, extension->name());

  // Now accept the repair prompt.
  AutoAcceptInstall();
  base::RunLoop run_loop2;
  reinstaller = base::MakeRefCounted<WebstoreReinstaller>(
      active_web_contents, kTestExtensionId,
      base::BindOnce(&WebstoreReinstallerBrowserTest::OnInstallCompletion,
                     base::Unretained(this), run_loop2.QuitClosure()));
  reinstaller->BeginReinstall();
  run_loop2.Run();

  // The reinstall should have succeeded, and the extension should have been
  // "updated" (which in this case means that it should have been replaced with
  // the inline install test extension, since that's the id we used).
  EXPECT_TRUE(last_install_result());
  extension = registry->enabled_extensions().GetByID(kTestExtensionId);
  ASSERT_TRUE(extension.get());
  // The name should not match, since the extension changed.
  EXPECT_NE(kExtensionName, extension->name());
}

}  // namespace extensions
