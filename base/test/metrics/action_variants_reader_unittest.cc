// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/action_variants_reader.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

constexpr char kTestActionXml[] = R"(
<actions>

  <variants name="TestVariants1">
    <variant name=".Variant1" summary="The first variant"/>
    <variant name=".Variant2" summary="The second variant"/>
  </variants>

  <variants name="TestVariants2">
    <variant name=".Variant3" summary="The third variant"/>
  </variants>

  <action name="Action.WithSingleToken">
    <token variants="TestVariants1"/>
  </action>

  <action name="Action.WithMultipleTokens">
    <token variants="TestVariants1"/>
    <token variants="TestVariants2"/>
  </action>

  <action name="Action.WithInlineToken">
    <token>
      <variant name=".InlineVariant" summary="An inline variant"/>
    </token>
  </action>

  <action name="Action.WithMixedTokens">
    <token variants="TestVariants1"/>
    <token>
      <variant name=".InlineVariant" summary="An inline variant"/>
    </token>
  </action>

  <action name="Action.WithNoTokens">
    <owner>test@google.com</owner>
    <description>An action with no variants.</description>
  </action>

</actions>
)";

TEST(ActionVariantsReaderTest, NoVariantsFound) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionXml, "Action.DoesNotExist", /*separator=*/"");
  EXPECT_TRUE(results.empty());
}

TEST(ActionVariantsReaderTest, ActionWithNoTokens) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionXml, "Action.WithNoTokens", /*separator=*/"");
  EXPECT_TRUE(results.empty());
}

TEST(ActionVariantsReaderTest, OneResult) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionXml, "Action.WithSingleToken", /*separator=*/".");
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(2U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "Variant1"));
  EXPECT_TRUE(Contains(results[0], "Variant2"));
}

TEST(ActionVariantsReaderTest, MultipleTokens) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionXml, "Action.WithMultipleTokens", /*separator=*/".");
  ASSERT_EQ(2U, results.size());
  EXPECT_EQ(2U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "Variant1"));
  EXPECT_TRUE(Contains(results[0], "Variant2"));
  EXPECT_EQ(1U, results[1].size());
  EXPECT_TRUE(Contains(results[1], "Variant3"));
}

TEST(ActionVariantsReaderTest, InlineToken) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionXml, "Action.WithInlineToken", /*separator=*/".");
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(1U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "InlineVariant"));
}

TEST(ActionVariantsReaderTest, MixedTokens) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionXml, "Action.WithMixedTokens", /*separator=*/"");
  ASSERT_EQ(2U, results.size());
  EXPECT_EQ(2U, results[0].size());
  EXPECT_TRUE(Contains(results[0], ".Variant1"));
  EXPECT_TRUE(Contains(results[0], ".Variant2"));
  EXPECT_EQ(1U, results[1].size());
  EXPECT_TRUE(Contains(results[1], ".InlineVariant"));
}
// TODO: crbug.com/374120501 - bring test back when actions.xml has variants.
// TEST(ActionVariantsReaderTest, CallActualMethod) {
//   // Sanity check to ensure that the non-test-only version of the
//   // function can be called and that it finds a real action.
//   const auto results =
//       ReadActionVariantsForAction(
//         "Actions.PinnedToolbarButtonActivation", /*separator=*/".");
//   EXPECT_FALSE(results.empty());
// }

constexpr char kTestActionWithSeparatorXml[] = R"(
<actions>
  <action name="Action.WithSeparator{ActionType}">
    <token key="ActionType">
      <variant name=".Variant1" summary="Test variant 1"/>
    </token>
  </action>
</actions>
)";

TEST(ActionVariantsReaderTest, DotSeparator) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionWithSeparatorXml, "Action.WithSeparator", /*separator=*/".");
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(1U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "Variant1"));
}

TEST(ActionVariantsReaderTest, NoSeparator) {
  const auto results = ReadActionVariantsForActionFromXmlString(
      kTestActionWithSeparatorXml, "Action.WithSeparator", /*separator=*/"");
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(1U, results[0].size());
  EXPECT_TRUE(Contains(results[0], ".Variant1"));
}

}  // namespace base::test
