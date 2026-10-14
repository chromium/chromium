// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/no_destructor.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"

namespace glic {
namespace {

bool IsIneligible(const std::string& host, const std::string& path) {
  return host.find("ineligible") != std::string::npos ||
         path.find("ineligible") != std::string::npos;
}

bool MockIsEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  return !IsIneligible(host, path);
}

bool MockIsEligibleWithAccount(
    const std::string& host,
    const std::string& path,
    const std::string& account,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  return !IsIneligible(host, path);
}

bool MockShouldReextractPageContext(
    const std::string& host,
    const std::string& path,
    const std::vector<std::string>& updated_meta_tags) {
  return false;
}

optimization_guide::StringViewSpan MockGetMeta(
    std::string_view host,
    std::string_view path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata) {
  return optimization_guide::StringViewSpan{.data = nullptr, .size = 0};
}

optimization_guide::PageEligibilityResult MockCheckPageEligibility(
    const std::vector<optimization_guide::FrameUrl>& frames) {
  bool eligible = true;
  for (const auto& frame : frames) {
    if (IsIneligible(frame.host, frame.path)) {
      eligible = false;
      break;
    }
  }
  return optimization_guide::PageEligibilityResult{
      .status = eligible ? optimization_guide::PageEligibility::kEligible
                         : optimization_guide::PageEligibility::kIneligible,
      .meta_tag_names_affecting_eligibility = {.data = nullptr, .size = 0}};
}

constexpr optimization_guide::PageContextEligibilityAPI kTestApi = {
    .IsPageContextEligible = &MockIsEligible,
    .IsPageContextEligibleWithAccount = &MockIsEligibleWithAccount,
    .ShouldReextractPageContext = &MockShouldReextractPageContext,
    .GetMetaTagNamesAffectingEligibility = &MockGetMeta,
    .CheckPageEligibility = &MockCheckPageEligibility,
};

}  // namespace

ScopedPageContextEligibilityForTesting::
    ScopedPageContextEligibilityForTesting() {
  static base::NoDestructor<optimization_guide::PageContextEligibility> holder(
      &kTestApi);
  optimization_guide::PageContextEligibility::SetForTesting(holder.get());
}

ScopedPageContextEligibilityForTesting::
    ~ScopedPageContextEligibilityForTesting() {
  optimization_guide::PageContextEligibility::SetForTesting(nullptr);
}

}  // namespace glic
