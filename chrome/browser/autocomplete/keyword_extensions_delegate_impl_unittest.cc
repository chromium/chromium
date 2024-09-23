// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/keyword_extensions_delegate_impl.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

class KeywordExtensionsDelegateImplTest : public ExtensionServiceTestBase {
 public:
  KeywordExtensionsDelegateImplTest() {}

  KeywordExtensionsDelegateImplTest(const KeywordExtensionsDelegateImplTest&) =
      delete;
  KeywordExtensionsDelegateImplTest& operator=(
      const KeywordExtensionsDelegateImplTest&) = delete;

  ~KeywordExtensionsDelegateImplTest() override {}

 protected:
  void SetUp() override;

  void RunTest(bool incognito);
};

void KeywordExtensionsDelegateImplTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeExtensionService(ExtensionServiceInitParams());
}

void KeywordExtensionsDelegateImplTest::RunTest(bool incognito) {
  search_engines::SearchEnginesTestEnvironment test_environment;
  MockAutocompleteProviderClient client;
  client.set_template_url_service(test_environment.template_url_service());
  scoped_refptr<KeywordProvider> keyword_provider =
      new KeywordProvider(&client, nullptr);

  // Load an extension.
  {
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
    path = path.AppendASCII("extensions").AppendASCII("simple_with_popup");

    TestExtensionRegistryObserver load_observer(registry());
    scoped_refptr<UnpackedInstaller> installer(
        UnpackedInstaller::Create(service()));
    installer->Load(path);
    EXPECT_TRUE(load_observer.WaitForExtensionInstalled());
  }

  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  scoped_refptr<const Extension> extension =
      *(registry()->enabled_extensions().begin());
  ASSERT_FALSE(util::IsIncognitoEnabled(extension->id(), profile()));

  Profile* profile_to_use =
      incognito ? profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                : profile();
  KeywordExtensionsDelegateImpl delegate_impl(profile_to_use,
                                              keyword_provider.get());
  KeywordExtensionsDelegate* delegate = &delegate_impl;
  EXPECT_NE(incognito, delegate->IsEnabledExtension(extension->id()));

  // Enable the extension in incognito mode, which requires a reload.
  {
    TestExtensionRegistryObserver observer(registry());
    util::SetIsIncognitoEnabled(extension->id(), profile(), true);
    EXPECT_TRUE(observer.WaitForExtensionInstalled());
  }

  ASSERT_EQ(1U, registry()->enabled_extensions().size());
  extension = *(registry()->enabled_extensions().begin());
  ASSERT_TRUE(util::IsIncognitoEnabled(extension->id(), profile()));
  EXPECT_TRUE(delegate->IsEnabledExtension(extension->id()));
}

TEST_F(KeywordExtensionsDelegateImplTest, IsEnabledExtension) {
  RunTest(false);
}

TEST_F(KeywordExtensionsDelegateImplTest, IsEnabledExtensionIncognito) {
  RunTest(true);
}

}  // namespace

}  // namespace extensions
