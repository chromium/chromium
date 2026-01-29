// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/siwg_button_finder.h"

#include "base/containers/fixed_flat_set.h"
#include "base/containers/map_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace actor_login {

namespace {

bool MatchesButtonAttributes(const RE2& regex,
                             const autofill::mojom::SiwgButtonData& button) {
  return RE2::PartialMatch(base::UTF16ToUTF8(button.text), regex) ||
         RE2::PartialMatch(base::UTF16ToUTF8(button.id_attribute), regex) ||
         RE2::PartialMatch(base::UTF16ToUTF8(button.class_attribute), regex) ||
         RE2::PartialMatch(base::UTF16ToUTF8(button.aria_label), regex) ||
         RE2::PartialMatch(base::UTF16ToUTF8(button.href_attribute), regex);
}

bool IsClickable(const optimization_guide::proto::ContentNode* node) {
  return !node->content_attributes()
              .interaction_info()
              .clickability_reasons()
              .empty();
}

}  // namespace

SiwgButtonFinder::SiwgButtonFinder(
    optimization_guide::proto::AnnotatedPageContent page_content)
    : page_content_(std::move(page_content)) {
  BuildContentNodeMaps(page_content_.root_node());
}

SiwgButtonFinder::~SiwgButtonFinder() = default;

void SiwgButtonFinder::BuildContentNodeMaps(
    const optimization_guide::proto::ContentNode& node) {
  dom_node_id_to_content_node_[node.content_attributes()
                                   .common_ancestor_dom_node_id()] = &node;
  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    parent_map_[&child] = &node;
    BuildContentNodeMaps(child);
  }
}

std::optional<int> SiwgButtonFinder::FindClosestClickableAncestor(
    const optimization_guide::proto::ContentNode& node) {
  const optimization_guide::proto::ContentNode* current_node = &node;
  while (current_node) {
    if (IsClickable(current_node)) {
      return current_node->content_attributes().common_ancestor_dom_node_id();
    }
    current_node = base::FindPtrOrNull(parent_map_, current_node);
  }

  return std::nullopt;
}

std::optional<int> SiwgButtonFinder::FindGoogleSdkButton(
    const GURL& button_frame_url,
    const std::vector<autofill::mojom::SiwgButtonDataPtr>& buttons) {
  // Google-recommended implementation of SiwG button uses a specific iframe.
  // The button element in this case is the <div> with role="button".
  // This is the highest confidence match.
  if (base::StartsWith(button_frame_url.spec(),
                       "https://accounts.google.com/gsi/button")) {
    for (const auto& button : buttons) {
      if (base::ToLowerASCII(button->tag_name) == u"div" &&
          base::ToLowerASCII(button->role) == u"button") {
        return button->dom_node_id;
      }
    }
  }
  return std::nullopt;
}

std::optional<int> SiwgButtonFinder::FindButton(
    const GURL& button_frame_url,
    const std::vector<autofill::mojom::SiwgButtonDataPtr>& buttons) {
  if (std::optional<int> button_id =
          FindGoogleSdkButton(button_frame_url, buttons)) {
    return button_id;
  }

  // Layer 1: High confidence, direct matching on button labels.
  // These are literal strings and can be translated in the future.
  static constexpr auto kHighConfidenceMatches =
      base::MakeFixedFlatSet<std::string_view>(
          {"continue with google", "sign in with google", "sign up with google",
           "log in with google", "login with google", "sign up using google",
           "sign in using google", "log in using google", "use google account",
           "sign in via google$"});

  // Layer 2: Broad regex matching for various attributes.
  // This is a wider regex to capture more cases, including developer-set ids
  // and classes. Less precise and cannot be translated easily.
  static const base::NoDestructor<RE2> kGoogleSigninButtonRegexMatcher([] {
    const std::vector<std::string_view> kSigninActionAtomics = {
        "sign.?in", "sign.?up", "log.?in",      "login",
        "auth",     "social",   "registration", "continue"};
    const std::string kSigninActionRegex =
        "(?:" + base::JoinString(kSigninActionAtomics, "|") + ")";
    return base::StrCat({"(?i)(?:google[-_.\\s]*.*", kSigninActionRegex,
                         ")|(?:", kSigninActionRegex,
                         ".*[-_.\\s]*(?:with[-_.\\s]*)?google)"});
  }());

  // Layer 3: Anti-matching to exclude false positives.
  // Explicitly filter out things related to other Google products.
  static const base::NoDestructor<RE2> kGoogleProductExclusionRegexMatcher(
      "(?i)classroom");

  for (const autofill::mojom::SiwgButtonDataPtr& button : buttons) {
    const optimization_guide::proto::ContentNode* node =
        base::FindPtrOrNull(dom_node_id_to_content_node_, button->dom_node_id);
    if (!node) {
      continue;
    }

    // High confidence direct matching on button text.
    if (kHighConfidenceMatches.contains(base::TrimWhitespaceASCII(
            base::UTF16ToUTF8(button->text), base::TRIM_ALL))) {
      if (std::optional<int> ancestor_id =
              FindClosestClickableAncestor(*node)) {
        return ancestor_id;
      }
    }

    // Regex matching on all attributes.
    if (MatchesButtonAttributes(*kGoogleSigninButtonRegexMatcher, *button)) {
      // Anti-matching to exclude false positives.
      if (MatchesButtonAttributes(*kGoogleProductExclusionRegexMatcher,
                                  *button)) {
        continue;
      }

      if (std::optional<int> ancestor_id =
              FindClosestClickableAncestor(*node)) {
        return ancestor_id;
      }
    }
  }

  return std::nullopt;
}

}  // namespace actor_login
