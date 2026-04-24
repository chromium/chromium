// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "pdf/mojom/pdf.mojom.h"

using testing::Each;
using testing::Field;
using testing::IsEmpty;
using testing::Ne;
using testing::Not;
using testing::Pointee;

namespace pdf::mojom {

void PrintTo(const pdf::mojom::InkGlyphInfo& info, std::ostream* os) {
  *os << "{.glyph = " << info.glyph << ", .offset = " << info.offset.ToString()
      << ", .total_advance = " << info.total_advance << "}";
}

void PrintTo(const pdf::mojom::InkGlyphInfoPtr& info, std::ostream* os) {
  if (!info) {
    *os << "nullptr";
    return;
  }
  PrintTo(*info, os);
}

}  // namespace pdf::mojom

namespace extensions {

namespace {

base::ListValue GenerateSampleAllowlist() {
  base::ListValue allowlist;
  allowlist.Append("allowed-domain.com");
  allowlist.Append("example.com");

  // Even though the allowlist is supposed to be a set of domains, it's possible
  // for it to contain garbage values. This should be handled gracefully.
  allowlist.Append(10);
  return allowlist;
}

}  // namespace

class PdfViewerPrivateApiTest : public ExtensionApiTest {
 public:
  // Set up policy values.
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::PolicyMap values;

    values.Set(policy::key::kPdfLocalFileAccessAllowedForDomains,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GenerateSampleAllowlist()), nullptr);
    policy_provider_.UpdateChromePolicy(values);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(PdfViewerPrivateApiTest, AccessControl) {
  // Runs the tests in access_control_test.js.
  ASSERT_TRUE(RunExtensionTest("pdf_viewer_private",
                               {.extension_url = "access_control_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PdfViewerPrivateApiTest, GetTextInfo) {
  // Runs the tests in text_info_test.js.
  ASSERT_TRUE(RunExtensionTest("pdf_viewer_private",
                               {.extension_url = "text_info_test.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PdfViewerPrivateApiTest, GetTextInfo_MojoSerialization) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("pdf_viewer_private"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(
      NavigateToURL(web_contents(), extension->GetResourceURL("empty.html")));
  content::EvalJsResult result = content::EvalJs(web_contents(), R"js(
    (async () => {
      const textarea = document.createElement('textarea');
      textarea.style = 'font-family: sans-serif'
      textarea.value = 'Mojo Test x\u0301';
      document.body.appendChild(textarea);
      return await chrome.pdfViewerPrivate.getTextInfo(textarea, []);
    })();
  )js");
  ASSERT_TRUE(result.is_dict());
  const base::DictValue& text_info_dict = result.ExtractDict();

  const std::vector<uint8_t>* text_info_blob =
      text_info_dict.FindBlob("mojoTextInfo");
  ASSERT_TRUE(text_info_blob);
  pdf::mojom::InkTextInfoPtr text_info;
  ASSERT_TRUE(
      pdf::mojom::InkTextInfo::Deserialize(*text_info_blob, &text_info));
  ASSERT_TRUE(text_info);

  // Check that every field is non-zero.
  EXPECT_NE(text_info->effective_zoom, 0);
  ASSERT_THAT(text_info->text_runs, Not(IsEmpty()));
  pdf::mojom::InkTextRunPtr& text_run = text_info->text_runs[0];
  EXPECT_NE(text_run->location, gfx::RectF());
  ASSERT_THAT(text_run->typeface_runs, Not(IsEmpty()));
  pdf::mojom::InkTypefaceRunPtr& typeface_run = text_run->typeface_runs[0];
  EXPECT_TRUE(typeface_run->is_horizontal);
  EXPECT_NE(typeface_run->typeface_id, 0);
  EXPECT_THAT(typeface_run->glyphs, Not(IsEmpty()));
  EXPECT_THAT(
      typeface_run->glyphs,
      Each(Pointee(Field("glyph", &pdf::mojom::InkGlyphInfo::glyph, Ne(0)))));
  EXPECT_THAT(
      typeface_run->glyphs,
      Contains(Pointee(Field("offset", &pdf::mojom::InkGlyphInfo::offset,
                             Ne(gfx::Vector2dF())))));
  EXPECT_THAT(
      typeface_run->glyphs,
      Contains(Pointee(Field(
          "total_advance", &pdf::mojom::InkGlyphInfo::total_advance, Ne(0)))));
}

}  // namespace extensions
