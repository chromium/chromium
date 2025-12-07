// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search/search_api.h"

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Creates an extension with "search" permission.
scoped_refptr<const Extension> CreateSearchExtension() {
  return ExtensionBuilder("Extension with search permission")
      .AddAPIPermission("search")
      .Build();
}

scoped_refptr<SearchQueryFunction> CreateSearchFunction(
    scoped_refptr<const Extension> extension) {
  auto function = base::MakeRefCounted<SearchQueryFunction>();
  function->set_extension(extension.get());
  function->set_has_callback(true);
  return function;
}

}  // namespace

class SearchApiBrowserTest : public ExtensionBrowserTest {
 protected:
  SearchApiBrowserTest() = default;
  ~SearchApiBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(profile()));
    scoped_refptr<const Extension> extension = CreateSearchExtension();
    function_ = CreateSearchFunction(extension);
  }

  void RunFunctionAndExpectError(const std::string& input,
                                 std::string_view expected) {
    auto result =
        api_test_utils::RunFunctionAndReturnError(function(), input, profile());
    EXPECT_EQ(expected, result);
  }

  extensions::SearchQueryFunction* function() { return function_.get(); }

  scoped_refptr<extensions::SearchQueryFunction> function_;
};

// Test for error if search field is empty string.
IN_PROC_BROWSER_TEST_F(SearchApiBrowserTest, QueryEmpty) {
  RunFunctionAndExpectError(R"([{"text": ""}])", "Empty text parameter.");
}

// Test for error if both disposition and tabId are populated.
IN_PROC_BROWSER_TEST_F(SearchApiBrowserTest, DispositionAndTabIDValid) {
  RunFunctionAndExpectError(
      R"([{"text": "1", "disposition": "NEW_TAB", "tabId": 1}])",
      "Cannot set both 'disposition' and 'tabId'.");
}

// Test for error if an invalid tabId is provided.
IN_PROC_BROWSER_TEST_F(SearchApiBrowserTest, InvalidTabId) {
  RunFunctionAndExpectError(R"([{"text": "1", "tabId": -1}])",
                            "No tab with id: -1.");
}

// Test for error if missing browser context.
IN_PROC_BROWSER_TEST_F(SearchApiBrowserTest, NoActiveBrowser) {
  auto result = api_test_utils::RunFunctionAndReturnError(
      function(), R"([{"text": "1"}])", nullptr);

  EXPECT_EQ("No active browser.", result);
}

}  // namespace extensions
