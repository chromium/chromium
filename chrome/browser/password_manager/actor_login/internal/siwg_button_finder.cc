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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"

namespace actor_login {

namespace {

// Helper function to create `ObservedToolTarget` mojom struct from
// `ContentAttributes`.
// This is duplicated code from `actor::PageTool`. The code is only needed for
// the E2E prototype and will be removed after.
// TODO(crbug.com/480920277): Remove this code after the click is handled by the
// `ExecutionEngine`.
actor::mojom::ObservedToolTargetPtr ToMojoObservedToolTarget(
    const optimization_guide::proto::ContentAttributes* attributes,
    content::RenderFrameHost* rfh) {
  if (!attributes) {
    return nullptr;
  }
  auto observed_target = actor::mojom::ObservedToolTarget::New();
  observed_target->node_attribute =
      blink::mojom::AIPageContentAttributes::New();

  if (attributes->has_common_ancestor_dom_node_id()) {
    observed_target->node_attribute->dom_node_id =
        attributes->common_ancestor_dom_node_id();
  }

  if (attributes->has_geometry()) {
    observed_target->node_attribute->geometry =
        blink::mojom::AIPageContentGeometry::New();
    const auto& geometry = attributes->geometry();
    const auto& outer_box = geometry.outer_bounding_box();
    gfx::Rect outer_rect(outer_box.x(), outer_box.y(), outer_box.width(),
                         outer_box.height());

    const auto& visible_box = geometry.visible_bounding_box();
    gfx::Rect visible_rect(visible_box.x(), visible_box.y(),
                           visible_box.width(), visible_box.height());

    if (rfh && rfh->GetView()) {
      const gfx::Point outer_box_origin_point = gfx::ToRoundedPoint(
          rfh->GetView()->TransformRootPointToViewCoordSpace(
              gfx::PointF(outer_rect.origin())));
      outer_rect.set_origin(outer_box_origin_point);

      const gfx::Point visible_box_origin_point = gfx::ToRoundedPoint(
          rfh->GetView()->TransformRootPointToViewCoordSpace(
              gfx::PointF(visible_rect.origin())));
      visible_rect.set_origin(visible_box_origin_point);
    }

    observed_target->node_attribute->geometry->outer_bounding_box = outer_rect;
    observed_target->node_attribute->geometry->visible_bounding_box =
        visible_rect;
  }
  return observed_target;
}

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

SiwgButtonFinder::SiwgButton::SiwgButton() = default;
SiwgButtonFinder::SiwgButton::SiwgButton(
    int dom_node_id,
    actor::mojom::ObservedToolTargetPtr observed_target)
    : dom_node_id(dom_node_id), observed_target(std::move(observed_target)) {}
SiwgButtonFinder::SiwgButton::~SiwgButton() = default;
SiwgButtonFinder::SiwgButton::SiwgButton(SiwgButton&&) = default;
SiwgButtonFinder::SiwgButton& SiwgButtonFinder::SiwgButton::operator=(
    SiwgButton&&) = default;

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

std::optional<SiwgButtonFinder::SiwgButton> SiwgButtonFinder::FindButton(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::mojom::SiwgButtonDataPtr>& buttons) {
  if (std::optional<int> button_id =
          FindGoogleSdkButton(rfh->GetLastCommittedURL(), buttons)) {
    return SiwgButton{*button_id, ToMojoObservedToolTarget(
                                      GetContentAttributes(*button_id), rfh)};
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
        return SiwgButton{
            *ancestor_id,
            ToMojoObservedToolTarget(GetContentAttributes(*ancestor_id), rfh)};
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
        return SiwgButton{
            *ancestor_id,
            ToMojoObservedToolTarget(GetContentAttributes(*ancestor_id), rfh)};
      }
    }
  }

  return std::nullopt;
}

const optimization_guide::proto::ContentAttributes*
SiwgButtonFinder::GetContentAttributes(int dom_node_id) const {
  const optimization_guide::proto::ContentNode* node =
      base::FindPtrOrNull(dom_node_id_to_content_node_, dom_node_id);
  return node ? &node->content_attributes() : nullptr;
}

}  // namespace actor_login
