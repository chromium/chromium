// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/siwg_button_finder.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor_login {
namespace {

using autofill::mojom::SiwgButtonData;
using autofill::mojom::SiwgButtonDataPtr;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::ContentNode;

SiwgButtonDataPtr CreateButtonData(int dom_node_id,
                                   const std::string& text = "",
                                   const std::string& tag_name = "",
                                   const std::string& role = "",
                                   const std::string& aria_label = "") {
  auto button = SiwgButtonData::New();
  button->dom_node_id = dom_node_id;
  button->text = base::UTF8ToUTF16(text);
  button->tag_name = base::UTF8ToUTF16(tag_name);
  button->role = base::UTF8ToUTF16(role);
  button->aria_label = base::UTF8ToUTF16(aria_label);
  return button;
}

ContentNode CreateContentNode(int dom_node_id, bool is_interactable) {
  ContentNode node;
  node.mutable_content_attributes()->set_common_ancestor_dom_node_id(
      dom_node_id);
  if (is_interactable) {
    node.mutable_content_attributes()
        ->mutable_interaction_info()
        ->add_clickability_reasons(
            optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL);
  }
  return node;
}

TEST(SiwgButtonFinderTest, FindButton_HighConfidenceMatch) {
  AnnotatedPageContent page_content;
  // Root node (interactable container) -> Button node
  *page_content.mutable_root_node() = CreateContentNode(1, true);
  *page_content.mutable_root_node()->add_children_nodes() =
      CreateContentNode(2, false);

  SiwgButtonFinder finder(std::move(page_content));

  std::vector<SiwgButtonDataPtr> buttons;
  buttons.push_back(CreateButtonData(2, "Sign in with Google"));

  EXPECT_EQ(finder.FindButton(GURL("https://example.com"), buttons), 1);
}

TEST(SiwgButtonFinderTest, FindButton_NoMatch) {
  AnnotatedPageContent page_content;
  *page_content.mutable_root_node() = CreateContentNode(1, true);
  *page_content.mutable_root_node()->add_children_nodes() =
      CreateContentNode(2, false);

  SiwgButtonFinder finder(std::move(page_content));

  std::vector<SiwgButtonDataPtr> buttons;
  buttons.push_back(CreateButtonData(2, "Random Button"));

  EXPECT_EQ(finder.FindButton(GURL("https://example.com"), buttons),
            std::nullopt);
}

TEST(SiwgButtonFinderTest, FindButton_NoInteractableAncestor) {
  AnnotatedPageContent page_content;
  *page_content.mutable_root_node() = CreateContentNode(1, false);
  *page_content.mutable_root_node()->add_children_nodes() =
      CreateContentNode(2, false);

  SiwgButtonFinder finder(std::move(page_content));

  std::vector<SiwgButtonDataPtr> buttons;
  buttons.push_back(CreateButtonData(2, "Sign in with Google"));

  EXPECT_EQ(finder.FindButton(GURL("https://example.com"), buttons),
            std::nullopt);
}

TEST(SiwgButtonFinderTest, FindButton_GoogleSdkIframe) {
  AnnotatedPageContent page_content;
  // Content structure doesn't matter much for this path as it returns button ID
  // directly.
  *page_content.mutable_root_node() = CreateContentNode(1, true);

  SiwgButtonFinder finder(std::move(page_content));

  std::vector<SiwgButtonDataPtr> buttons;
  // Matches: div + role="button"
  buttons.push_back(CreateButtonData(10, "", "div", "button"));
  // Mismatch
  buttons.push_back(CreateButtonData(11, "", "span", "button"));

  // URL matches the specific Google SDK iframe URL.
  EXPECT_EQ(finder.FindButton(
                GURL("https://accounts.google.com/gsi/button?client_id=..."),
                buttons),
            10);
}

TEST(SiwgButtonFinderTest, FindButton_AttributeMatch) {
  AnnotatedPageContent page_content;
  *page_content.mutable_root_node() = CreateContentNode(1, true);
  *page_content.mutable_root_node()->add_children_nodes() =
      CreateContentNode(2, false);

  SiwgButtonFinder finder(std::move(page_content));

  std::vector<SiwgButtonDataPtr> buttons;
  // "sign in" regex match in aria-label
  buttons.push_back(
      CreateButtonData(2, "Icon", "button", "", "Sign in with Google"));

  EXPECT_EQ(finder.FindButton(GURL("https://example.com"), buttons), 1);
}

}  // namespace
}  // namespace actor_login
