// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_api.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"

namespace glic {

namespace {

class PdfViewerPrivateBrowserTestGlicEnabled : public GlicBrowserTest {
 public:
  PdfViewerPrivateBrowserTestGlicEnabled() = default;
  ~PdfViewerPrivateBrowserTestGlicEnabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfGlicSummarize};
};

IN_PROC_BROWSER_TEST_F(PdfViewerPrivateBrowserTestGlicEnabled, GlicSummarize) {
  auto* tab = GetTabListInterface()->GetActiveTab();
  auto function =
      base::MakeRefCounted<extensions::PdfViewerPrivateGlicSummarizeFunction>();
  function->SetRenderFrameHost(tab->GetContents()->GetPrimaryMainFrame());

  auto expected = extensions::api_test_utils::RunFunctionAndReturnExpected(
      function.get(), "[]", GetProfile());
  EXPECT_TRUE(expected.has_value());
  EXPECT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kShown));
}

using PdfViewerPrivateBrowserTestGlicDisabled = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PdfViewerPrivateBrowserTestGlicDisabled, GlicSummarize) {
  auto* tab = GetTabListInterface()->GetActiveTab();
  auto function =
      base::MakeRefCounted<extensions::PdfViewerPrivateGlicSummarizeFunction>();
  function->SetRenderFrameHost(tab->GetContents()->GetPrimaryMainFrame());

  std::string error = extensions::api_test_utils::RunFunctionAndReturnError(
      function.get(), "[]", GetProfile());
  EXPECT_EQ("Glic is not enabled.", error);
}

}  // namespace
}  // namespace glic
