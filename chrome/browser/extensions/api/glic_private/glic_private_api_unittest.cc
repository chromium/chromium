// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class GlicPrivateApiGeminiEnterpriseTest : public ExtensionApiUnittest {
 protected:
  // Commits a navigation in a new tab and returns its main frame's document
  // ID.
  std::string NavigateAndGetDocumentId(const GURL& url) {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), /*instance=*/nullptr);
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(url);
    return ExtensionApiFrameIdMap::GetDocumentId(
               web_contents_->GetPrimaryMainFrame())
        .ToString();
  }

  void TearDown() override {
    web_contents_.reset();
    ExtensionApiUnittest::TearDown();
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  glic::ScopedGlicProviderOverride provider_override_{
      glic::GlicProvider::kGeminiEnterprise};
  base::test::ScopedFeatureList feature_list_{
      {extensions_features::kApiGlicPrivate,
       extensions_features::kApiGlicAccessFromGoogleWebpage,
       extensions_features::kApiGlicAccessFromPromotionPage,
       extensions_features::kApiGlicAccessFromWebContinuity}};
};

// Gemini Enterprise doesn't support Google pages invoking Glic, even when the
// per-source access features are enabled.
TEST_F(GlicPrivateApiGeminiEnterpriseTest,
       InvokeRejectedWithAccessFromPageDisabled) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting
      bypass_enablement_checks;
  EXPECT_EQ(
      RunFunctionAndReturnError(
          base::MakeRefCounted<GlicPrivateInvokeFunction>(),
          R"([{"documentId": "doc-id", "invocationSource": "promotion-page"}])"),
      "local-glic-access-from-page-disabled");
  EXPECT_EQ(
      RunFunctionAndReturnError(
          base::MakeRefCounted<GlicPrivateInvokeFunction>(),
          R"([{"documentId": "doc-id", "invocationSource": "universal-cart", "promptId": "p1"}])"),
      "local-glic-access-from-page-disabled");
  EXPECT_EQ(
      RunFunctionAndReturnError(
          base::MakeRefCounted<GlicPrivateInvokeFunction>(),
          R"([{"documentId": "doc-id", "invocationSource": "web-continuity"}])"),
      "local-glic-access-from-page-disabled");
}

// Gemini Enterprise reports that no Google page invocation source is enabled,
// even when the per-source access features are enabled.
TEST_F(GlicPrivateApiGeminiEnterpriseTest,
       GetStateReportsInvocationSourceDisabled) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting
      bypass_enablement_checks;
  const std::string document_id =
      NavigateAndGetDocumentId(GURL("https://example.com/"));
  for (const char* source :
       {"universal-cart", "promotion-page", "web-continuity"}) {
    SCOPED_TRACE(source);
    std::optional<base::DictValue> state = RunFunctionAndReturnDictionary(
        base::MakeRefCounted<GlicPrivateGetStateFunction>(),
        base::StringPrintf(R"(["%s", {"invocationSource": "%s"}])",
                           document_id.c_str(), source));
    ASSERT_TRUE(state);
    EXPECT_EQ(state->FindBool("invocationSourceEnabled"), false);
  }
}

}  // namespace
}  // namespace extensions
