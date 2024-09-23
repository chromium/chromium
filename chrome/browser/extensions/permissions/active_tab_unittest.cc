// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"

using extensions::mojom::APIPermissionID;

namespace extensions {
namespace {

scoped_refptr<const Extension> CreateTestExtension(
    const std::string& name,
    bool has_active_tab_permission,
    bool has_tab_capture_permission) {
  ExtensionBuilder builder(name);
  if (has_active_tab_permission) {
    builder.AddAPIPermission("activeTab");
  }
  if (has_tab_capture_permission) {
    builder.AddAPIPermission("tabCapture");
  }

  return builder.Build();
}

enum PermittedFeature {
  PERMITTED_NONE,
  PERMITTED_SCRIPT_ONLY,
  PERMITTED_CAPTURE_ONLY,
  PERMITTED_BOTH
};

class ActiveTabTest : public ChromeRenderViewHostTestHarness {
 protected:
  ActiveTabTest()
      : current_channel(version_info::Channel::DEV),
        extension(CreateTestExtension("deadbeef", true, false)),
        another_extension(CreateTestExtension("feedbeef", true, false)),
        extension_without_active_tab(
            CreateTestExtension("badbeef", false, false)),
        extension_with_tab_capture(
            CreateTestExtension("cafebeef", true, true)) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TabHelper::CreateForWebContents(web_contents());

    // We need to add extensions to the ExtensionService; else trying to commit
    // any of their URLs fails and redirects to about:blank.
    ExtensionService* service =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))
            ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                     base::FilePath(), false);
    service->AddExtension(extension.get());
    service->AddExtension(another_extension.get());
    service->AddExtension(extension_without_active_tab.get());
    service->AddExtension(extension_with_tab_capture.get());
  }

  int tab_id() {
    return sessions::SessionTabHelper::IdForTab(web_contents()).id();
  }

  ActiveTabPermissionGranter* active_tab_permission_granter() {
    return TabHelper::FromWebContents(web_contents())
        ->active_tab_permission_granter();
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension_refptr,
                 const GURL& url) {
    return IsAllowed(extension_refptr, url, PERMITTED_BOTH, tab_id());
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension_refptr,
                 const GURL& url,
                 PermittedFeature feature) {
    return IsAllowed(extension_refptr, url, feature, tab_id());
  }

  bool IsAllowed(const scoped_refptr<const Extension>& extension_refptr,
                 const GURL& url,
                 PermittedFeature feature,
                 int tab_id) {
    const PermissionsData* permissions_data =
        extension_refptr->permissions_data();
    bool script =
        permissions_data->CanAccessPage(url, tab_id, nullptr) &&
        permissions_data->CanRunContentScriptOnPage(url, tab_id, nullptr);
    bool capture = permissions_data->CanCaptureVisiblePage(
        url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls);
    switch (feature) {
      case PERMITTED_SCRIPT_ONLY:
        return script && !capture;
      case PERMITTED_CAPTURE_ONLY:
        return capture && !script;
      case PERMITTED_BOTH:
        return script && capture;
      case PERMITTED_NONE:
        return !script && !capture;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension_refptr,
                 const GURL& url) {
    return IsBlocked(extension_refptr, url, tab_id());
  }

  bool IsBlocked(const scoped_refptr<const Extension>& extension_refptr,
                 const GURL& url,
                 int tab_id) {
    return IsAllowed(extension_refptr, url, PERMITTED_NONE, tab_id);
  }

  bool HasTabsPermission(
      const scoped_refptr<const Extension>& extension_refptr) {
    return HasTabsPermission(extension_refptr, tab_id());
  }

  bool HasTabsPermission(const scoped_refptr<const Extension>& extension_refptr,
                         int tab_id) {
    return extension_refptr->permissions_data()->HasAPIPermissionForTab(
        tab_id, APIPermissionID::kTab);
  }

  bool IsGrantedForTab(const Extension* extension_refptr,
                       const content::WebContents* web_contents) {
    return extension_refptr->permissions_data()->HasAPIPermissionForTab(
        sessions::SessionTabHelper::IdForTab(web_contents).id(),
        APIPermissionID::kTab);
  }

  // TODO(justinlin): Remove when tabCapture is moved to stable.
  ScopedCurrentChannel current_channel;

  // An extension with the activeTab permission.
  scoped_refptr<const Extension> extension;

  // Another extension with activeTab (for good measure).
  scoped_refptr<const Extension> another_extension;

  // An extension without the activeTab permission.
  scoped_refptr<const Extension> extension_without_active_tab;

  // An extension with both the activeTab and tabCapture permission.
  scoped_refptr<const Extension> extension_with_tab_capture;
};

TEST_F(ActiveTabTest, GrantToSinglePage) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  // No access unless it's been granted.
  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_FALSE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));

  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab.get());

  // Granted to extension and extension_without_active_tab, but the latter
  // doesn't have the activeTab permission so not granted.
  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Other subdomains shouldn't be given access.
  GURL mail_google("http://mail.google.com");
  EXPECT_TRUE(IsBlocked(extension, mail_google));
  EXPECT_TRUE(IsBlocked(another_extension, mail_google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, mail_google));

  // Reloading the page should not clear the active permissions, since the
  // user remains on the same site.
  content::NavigationSimulator::Reload(web_contents());

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));

  // And grant a few more times redundantly for good measure.
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  // Navigating to a new URL should clear the active permissions.
  GURL chromium("http://www.chromium.org");
  NavigateAndCommit(chromium);

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));

  EXPECT_FALSE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));

  // Should be able to grant to multiple extensions at the same time (if they
  // have the activeTab permission, of course).
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab.get());

  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));

  // Should be able to go back to URLs that were previously cleared.
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());
  active_tab_permission_granter()->GrantIfRequested(
      extension_without_active_tab.get());

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));

  EXPECT_TRUE(IsBlocked(extension, chromium));
  EXPECT_TRUE(IsBlocked(another_extension, chromium));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, chromium));
}

TEST_F(ActiveTabTest, CapturingPagesWithActiveTab) {
  std::vector<GURL> test_urls = {
      GURL("https://example.com"),
      GURL(chrome::kChromeUIVersionURL),
      GURL(chrome::kChromeUINewTabURL),
      GURL("http://[2607:f8b0:4005:805::200e]"),
      ExtensionsClient::Get()->GetWebstoreBaseURL(),
      ExtensionsClient::Get()->GetNewWebstoreBaseURL(),
      extension->GetResourceURL("test.html"),
      another_extension->GetResourceURL("test.html"),
  };

  const GURL kAboutBlank("about:blank");

  for (const GURL& url : test_urls) {
    SCOPED_TRACE(url);
    NavigateAndCommit(url);
    EXPECT_EQ(url, web_contents()->GetLastCommittedURL());
    // By default, there should be no access.
    EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
        url, tab_id(), nullptr /*error*/,
        CaptureRequirement::kActiveTabOrAllUrls));
    // Granting permission should allow page capture.
    active_tab_permission_granter()->GrantIfRequested(extension.get());
    EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
        url, tab_id(), nullptr /*error*/,
        CaptureRequirement::kActiveTabOrAllUrls));
    // Navigating away should revoke access.
    NavigateAndCommit(kAboutBlank);
    EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
        url, tab_id(), nullptr /*error*/,
        CaptureRequirement::kActiveTabOrAllUrls));
  }
}

TEST_F(ActiveTabTest, Unloading) {
  // Some semi-arbitrary setup.
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsGrantedForTab(extension.get(), web_contents()));
  EXPECT_TRUE(IsAllowed(extension, google));

  // Unloading the extension should clear its tab permissions.
  ExtensionSystem::Get(web_contents()->GetBrowserContext())
      ->extension_service()
      ->DisableExtension(extension->id(), disable_reason::DISABLE_USER_ACTION);

  // Note: can't EXPECT_FALSE(IsAllowed) here because uninstalled extensions
  // are just that... considered to be uninstalled, and the manager might
  // just ignore them from here on.

  // Granting the extension again should give them back.
  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsGrantedForTab(extension.get(), web_contents()));
  EXPECT_TRUE(IsAllowed(extension, google));
}

TEST_F(ActiveTabTest, OnlyActiveTab) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_TRUE(IsAllowed(extension, google, PERMITTED_BOTH, tab_id()));
  EXPECT_TRUE(IsBlocked(extension, google, tab_id() + 1));
  EXPECT_FALSE(HasTabsPermission(extension, tab_id() + 1));
}

TEST_F(ActiveTabTest, SameDocumentNavigations) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  // Perform a same-document navigation. The extension should not lose the
  // temporary permission.
  GURL google_h1("http://www.google.com#h1");
  NavigateAndCommit(google_h1);

  EXPECT_TRUE(IsAllowed(extension, google));
  EXPECT_TRUE(IsAllowed(extension, google_h1));

  GURL chromium("http://www.chromium.org");
  NavigateAndCommit(chromium);

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_FALSE(IsAllowed(extension, chromium));

  active_tab_permission_granter()->GrantIfRequested(extension.get());

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_TRUE(IsAllowed(extension, chromium));

  GURL chromium_h1("http://www.chromium.org#h1");
  NavigateAndCommit(chromium_h1);

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(extension, chromium_h1));

  content::NavigationSimulator::Reload(web_contents());

  EXPECT_FALSE(IsAllowed(extension, google));
  EXPECT_FALSE(IsAllowed(extension, google_h1));
  EXPECT_TRUE(IsAllowed(extension, chromium));
  EXPECT_TRUE(IsAllowed(extension, chromium_h1));
}

TEST_F(ActiveTabTest, ChromeUrlGrants) {
  GURL internal(chrome::kChromeUIVersionURL);
  NavigateAndCommit(internal);
  active_tab_permission_granter()->GrantIfRequested(
      extension_with_tab_capture.get());
  // Do not grant tabs/hosts permissions for tab.
  EXPECT_TRUE(
      IsAllowed(extension_with_tab_capture, internal, PERMITTED_CAPTURE_ONLY));
  const PermissionsData* permissions_data =
      extension_with_tab_capture->permissions_data();
  EXPECT_TRUE(permissions_data->HasAPIPermissionForTab(
      tab_id(), APIPermissionID::kTabCaptureForTab));

  EXPECT_TRUE(IsBlocked(extension_with_tab_capture, internal, tab_id() + 1));
  EXPECT_FALSE(permissions_data->HasAPIPermissionForTab(
      tab_id() + 1, APIPermissionID::kTabCaptureForTab));
}

// Tests that an extension can have it's active tab permission cleared.
TEST_F(ActiveTabTest, ClearActiveExtensionAndNotify) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  // Grant access to the extension.
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  ASSERT_TRUE(IsAllowed(extension, google));
  ASSERT_TRUE(HasTabsPermission(extension));

  // Clear and confirm access was removed.
  active_tab_permission_granter()->ClearActiveExtensionAndNotify(
      extension->id());
  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_FALSE(HasTabsPermission(extension));
}

// Tests that clearing all extensions active tab permissions removes it for only
// those that had active tab and doesn't affect others.
TEST_F(ActiveTabTest, ClearAllActiveExtensionsAndNotify) {
  GURL google("http://www.google.com");
  NavigateAndCommit(google);

  // Grant access to two extensions.
  active_tab_permission_granter()->GrantIfRequested(extension.get());
  active_tab_permission_granter()->GrantIfRequested(another_extension.get());

  // Only the two active extensionsnow have access.
  ASSERT_TRUE(IsAllowed(extension, google));
  ASSERT_TRUE(IsAllowed(another_extension, google));
  ASSERT_TRUE(IsBlocked(extension_without_active_tab, google));
  ASSERT_TRUE(HasTabsPermission(extension));
  ASSERT_TRUE(HasTabsPermission(another_extension));
  ASSERT_FALSE(HasTabsPermission(extension_without_active_tab));

  // Revoke access for all granted extensions.
  active_tab_permission_granter()->RevokeForTesting();

  // None of the extensions have access anymore.
  EXPECT_TRUE(IsBlocked(extension, google));
  EXPECT_TRUE(IsBlocked(another_extension, google));
  EXPECT_TRUE(IsBlocked(extension_without_active_tab, google));
  EXPECT_FALSE(HasTabsPermission(extension));
  EXPECT_FALSE(HasTabsPermission(another_extension));
  EXPECT_FALSE(HasTabsPermission(extension_without_active_tab));
}

// An active tab test that includes an ExtensionService.
class ActiveTabWithServiceTest : public ExtensionServiceTestBase {
 public:
  ActiveTabWithServiceTest() {}

  ActiveTabWithServiceTest(const ActiveTabWithServiceTest&) = delete;
  ActiveTabWithServiceTest& operator=(const ActiveTabWithServiceTest&) = delete;

  void SetUp() override;
};

void ActiveTabWithServiceTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
}

// Tests that an extension can only capture file:// URLs with the active tab
// permission when it has file access granted.
// Regression test for https://crbug.com/810220.
TEST_F(ActiveTabWithServiceTest, FileURLs) {
  InitializeEmptyExtensionService();

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
    {
      "name": "Active Tab Capture With File Urls",
      "description": "Testing activeTab on file urls",
      "version": "0.1",
      "manifest_version": 2,
      "permissions": ["activeTab"]
    })");

  ChromeTestExtensionLoader loader(profile());
  loader.set_allow_file_access(false);
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const std::string id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

  EXPECT_FALSE(util::AllowFileAccess(id, profile()));

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  ASSERT_TRUE(web_contents);

  const GURL file_url("file:///foo");
  ASSERT_TRUE(content::WebContentsTester::For(web_contents.get()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(file_url);
  EXPECT_EQ(file_url, web_contents->GetLastCommittedURL());

  TabHelper::CreateForWebContents(web_contents.get());
  ActiveTabPermissionGranter* permission_granter =
      TabHelper::FromWebContents(web_contents.get())
          ->active_tab_permission_granter();
  ASSERT_TRUE(permission_granter);
  const int tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get()).id();
  EXPECT_NE(extension_misc::kUnknownTabId, tab_id);

  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      web_contents->GetLastCommittedURL(), tab_id, nullptr,
      CaptureRequirement::kActiveTabOrAllUrls));

  permission_granter->GrantIfRequested(extension.get());
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      web_contents->GetLastCommittedURL(), tab_id, nullptr,
      CaptureRequirement::kActiveTabOrAllUrls));

  permission_granter->RevokeForTesting();
  TestExtensionRegistryObserver observer(registry(), id);
  // This will reload the extension, so we need to reset the extension pointer.
  util::SetAllowFileAccess(id, profile(), true);
  extension = observer.WaitForExtensionLoaded();
  ASSERT_TRUE(extension);

  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      web_contents->GetLastCommittedURL(), tab_id, nullptr,
      CaptureRequirement::kActiveTabOrAllUrls));
  permission_granter->GrantIfRequested(extension.get());
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      web_contents->GetLastCommittedURL(), tab_id, nullptr,
      CaptureRequirement::kActiveTabOrAllUrls));
}

}  // namespace
}  // namespace extensions
