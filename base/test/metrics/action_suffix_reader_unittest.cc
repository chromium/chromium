// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/action_suffix_reader.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

constexpr char kTestActionXml[] = R"(
<actions>

  <action name="Action1">
    <owner>notarealuser@google.com</owner>
    <description>This is an action</description>
  </action>

  <action-suffix separator="." ordering="suffix">
    <suffix name="Suffix1" label="The first suffix"/>
    <affected-action name="Action.WithSuffix1"/>
    <suffix name="Suffix2" label="The second suffix"/>
    <affected-action name="Action.WithSuffix2"/>
  </action-suffix>

  <action-suffix separator="." ordering="suffix">
    <suffix name="Suffix3" label="The third suffix"/>
    <affected-action name="Action.WithSuffix3"/>
  </action-suffix>

  <action-suffix separator="." ordering="suffix">
    <affected-action name="Action.WithSuffix1"/>
    <suffix name="Suffix4" label="The fourth suffix"/>
  </action-suffix>

</actions>
)";

// Forward declare the private entry point for testing. This prevents having to
// import XmlReader, which is visible from base::test_support, but not
// base_unittests.
extern std::vector<ActionSuffixEntryMap> ReadActionSuffixesForActionForTesting(
    const std::string& xml_string,
    const std::string& affected_action);

TEST(ActionSuffixReaderTest, NoSuffixesFound) {
  const auto results = ReadActionSuffixesForActionForTesting(
      kTestActionXml, "Action.DoesNotExist");
  EXPECT_TRUE(results.empty());
}

TEST(ActionSuffixReaderTest, OneResult) {
  const auto results = ReadActionSuffixesForActionForTesting(
      kTestActionXml, "Action.WithSuffix3");
  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(1U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "Suffix3"));
}

TEST(ActionSuffixReaderTest, OneResultFromBlockWithOtherActions) {
  const auto results = ReadActionSuffixesForActionForTesting(
      kTestActionXml, "Action.WithSuffix2");
  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(2U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "Suffix1"));
  EXPECT_TRUE(Contains(results[0], "Suffix2"));
}

TEST(ActionSuffixReaderTest, MultipleResults) {
  const auto results = ReadActionSuffixesForActionForTesting(
      kTestActionXml, "Action.WithSuffix1");
  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(2U, results[0].size());
  EXPECT_TRUE(Contains(results[0], "Suffix1"));
  EXPECT_TRUE(Contains(results[0], "Suffix2"));
  EXPECT_EQ(1U, results[1].size());
  EXPECT_TRUE(Contains(results[1], "Suffix4"));
}

TEST(ActionSuffixReaderTest, CallActualMethod) {
  const auto results = ReadActionSuffixesForAction("Action.DoesNotExist");
  EXPECT_TRUE(results.empty());
}

}  // namespace base
