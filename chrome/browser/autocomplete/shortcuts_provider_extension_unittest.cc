// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_provider.h"
#include "components/omnibox/browser/shortcuts_provider_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

using ExpectedURLs = std::vector<ExpectedURLAndAllowedToBeDefault>;

namespace {

struct TestShortcutData shortcut_test_db[] = {
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F1", "echo echo", "echo echo",
     "chrome-extension://cedabbhfglmiikkmdgcpjdkocfcmbkee/?q=echo",
     AutocompleteMatch::DocumentType::NONE, "Run Echo command: echo", "0,0",
     "Echo echo", "0,4", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::EXTENSION_APP_DEPRECATED, "", 1, 1},
};

}  // namespace

// ShortcutsProviderExtensionTest ---------------------------------------------

class ShortcutsProviderExtensionTest : public testing::Test {
 public:
  ShortcutsProviderExtensionTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ChromeAutocompleteProviderClient> client_;
  scoped_refptr<ShortcutsBackend> backend_;
  scoped_refptr<ShortcutsProvider> provider_;
};

void ShortcutsProviderExtensionTest::SetUp() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  profile_ = profile_builder.Build();

  ShortcutsBackendFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(
          &ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting));

  client_ = std::make_unique<ChromeAutocompleteProviderClient>(profile_.get());
  backend_ = ShortcutsBackendFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(backend_.get());
  provider_ = new ShortcutsProvider(client_.get());
  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       shortcut_test_db,
                                       std::size(shortcut_test_db));
}

void ShortcutsProviderExtensionTest::TearDown() {
  // Run all pending tasks or else some threads hold on to the message loop
  // and prevent it from being deleted.
  task_environment_.RunUntilIdle();
}

// Actual tests ---------------------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ShortcutsProviderExtensionTest, Extension) {
  // Try an input string that matches an extension URL.
  std::u16string text(u"echo");
  std::string expected_url(
      "chrome-extension://cedabbhfglmiikkmdgcpjdkocfcmbkee/?q=echo");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u" echo");

  // Claim the extension has been unloaded.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Echo")
          .SetID("cedabbhfglmiikkmdgcpjdkocfcmbkee")
          .Build();
  extensions::ExtensionRegistry::Get(profile_.get())
      ->TriggerOnUnloaded(extension.get(),
                          extensions::UnloadedExtensionReason::UNINSTALL);

  // Now the URL should have disappeared.
  RunShortcutsProviderTest(provider_, text, false, ExpectedURLs(),
                           std::string(), std::u16string());
}
#endif
