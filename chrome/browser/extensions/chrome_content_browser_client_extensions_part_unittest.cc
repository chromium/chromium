// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

scoped_refptr<const Extension> BuildExtension(
    const std::string& name,
    const std::vector<std::string>& permissions) {
  return ExtensionBuilder(name)
      .SetManifestVersion(3)
      .AddAPIPermissions(permissions)
      .Build();
}

}  // namespace

// The browser-side checks behind navigator.clipboard in an extension service
// worker. They are the only thing standing between a compromised renderer and
// the system clipboard, so each way they can refuse is covered here; the
// browser tests in extension_dom_clipboard_apitest.cc exercise the renderer
// path, which rejects for its own reasons and so cannot fail if these regress.
class ClipboardForServiceWorkerTest : public ExtensionServiceTestBase {
 public:
  ClipboardForServiceWorkerTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kClipboardOnExtensionServiceWorker);
  }

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ClipboardForServiceWorkerTest, WriteAllowedWithPermission) {
  scoped_refptr<const Extension> extension =
      BuildExtension("allowed", {"clipboardWrite"});
  registrar()->AddExtension(extension.get());

  EXPECT_TRUE(
      ChromeContentBrowserClientExtensionsPart::
          ExtensionHasClipboardWritePermission(profile(), extension->id()));
}

TEST_F(ClipboardForServiceWorkerTest, WriteRefusedWithoutPermission) {
  scoped_refptr<const Extension> extension = BuildExtension("no-perm", {});
  registrar()->AddExtension(extension.get());

  EXPECT_FALSE(
      ChromeContentBrowserClientExtensionsPart::
          ExtensionHasClipboardWritePermission(profile(), extension->id()));
}

// The permission travels with the extension, so disabling it has to take the
// clipboard away from a worker that is already running.
TEST_F(ClipboardForServiceWorkerTest, WriteRefusedForDisabledExtension) {
  scoped_refptr<const Extension> extension =
      BuildExtension("allowed", {"clipboardWrite"});
  registrar()->AddExtension(extension.get());
  registrar()->DisableExtension(extension->id(),
                                {disable_reason::DISABLE_USER_ACTION});

  EXPECT_FALSE(
      ChromeContentBrowserClientExtensionsPart::
          ExtensionHasClipboardWritePermission(profile(), extension->id()));
}

TEST_F(ClipboardForServiceWorkerTest, WriteRefusedForUnknownExtension) {
  EXPECT_FALSE(ChromeContentBrowserClientExtensionsPart::
                   ExtensionHasClipboardWritePermission(
                       profile(), "abcdefghijklmnopabcdefghijklmnop"));
}

// The bind-time gate, which decides whether a worker gets a ClipboardHost at
// all. It is deliberately coarser than the write check above.
TEST_F(ClipboardForServiceWorkerTest, BindAllowedForExtensionOrigin) {
  EXPECT_TRUE(ChromeContentBrowserClientExtensionsPart::
                  IsClipboardAllowedForServiceWorker(url::Origin::Create(
                      GURL("chrome-extension://abcdefg/"))));
}

TEST_F(ClipboardForServiceWorkerTest, BindRefusedForWebOrigin) {
  EXPECT_FALSE(ChromeContentBrowserClientExtensionsPart::
                   IsClipboardAllowedForServiceWorker(
                       url::Origin::Create(GURL("https://example.com/"))));
}

TEST_F(ClipboardForServiceWorkerTest, BindRefusedWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(
      blink::features::kClipboardOnExtensionServiceWorker);

  EXPECT_FALSE(ChromeContentBrowserClientExtensionsPart::
                   IsClipboardAllowedForServiceWorker(url::Origin::Create(
                       GURL("chrome-extension://abcdefg/"))));
}

}  // namespace extensions
