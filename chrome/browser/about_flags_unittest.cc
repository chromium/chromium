// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_test_helpers.h"
#include "components/flags_ui/flags_ui_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace about_flags {

namespace {

using Sample = base::HistogramBase::Sample;
using SwitchToIdMap = std::map<std::string, Sample>;

// Get all associated switches corresponding to defined about_flags.cc entries.
std::set<std::string> GetAllPublicSwitchesAndFeaturesForTesting() {
  std::set<std::string> result;

  size_t num_entries = 0;
  const flags_ui::FeatureEntry* entries =
      testing::GetFeatureEntries(&num_entries);

  for (size_t i = 0; i < num_entries; ++i) {
    const flags_ui::FeatureEntry& entry = entries[i];

    // Skip over flags that are part of the flags system itself - they don't
    // have any of the usual metadata or histogram entries for flags, since they
    // are synthesized during the build process.
    // TODO(https://crbug.com/1068258): Remove the need for this by generating
    // histogram entries automatically.
    if (entry.supported_platforms & flags_ui::kFlagInfrastructure)
      continue;

    switch (entry.type) {
      case flags_ui::FeatureEntry::SINGLE_VALUE:
      case flags_ui::FeatureEntry::SINGLE_DISABLE_VALUE:
        result.insert(entry.switches.command_line_switch);
        break;
      case flags_ui::FeatureEntry::ORIGIN_LIST_VALUE:
        // Do nothing, origin list values are not added as feature flags.
        break;
      case flags_ui::FeatureEntry::MULTI_VALUE:
        for (int j = 0; j < entry.NumOptions(); ++j) {
          result.insert(entry.ChoiceForOption(j).command_line_switch);
        }
        break;
      case flags_ui::FeatureEntry::ENABLE_DISABLE_VALUE:
        result.insert(entry.switches.command_line_switch);
        result.insert(entry.switches.disable_command_line_switch);
        break;
      case flags_ui::FeatureEntry::FEATURE_VALUE:
      case flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
        result.insert(std::string(entry.feature.feature->name) + ":enabled");
        result.insert(std::string(entry.feature.feature->name) + ":disabled");
        break;
    }
  }
  return result;
}

}  // anonymous namespace

// Makes sure there are no separators in any of the entry names.
TEST(AboutFlagsTest, NoSeparators) {
  size_t count;
  const flags_ui::FeatureEntry* entries = testing::GetFeatureEntries(&count);
  for (size_t i = 0; i < count; ++i) {
    std::string name = entries[i].internal_name;
    EXPECT_EQ(std::string::npos, name.find(flags_ui::testing::kMultiSeparator))
        << i;
  }
}

// Makes sure that every flag has an owner and an expiry entry in
// flag-metadata.json.
TEST(AboutFlagsTest, EveryFlagHasMetadata) {
  size_t count;
  const flags_ui::FeatureEntry* entries = testing::GetFeatureEntries(&count);
  flags_ui::testing::EnsureEveryFlagHasMetadata(
      base::make_span(entries, count));
}

// Ensures that all flags marked as never expiring in flag-metadata.json is
// listed in flag-never-expire-list.json.
TEST(AboutFlagsTest, OnlyPermittedFlagsNeverExpire) {
  flags_ui::testing::EnsureOnlyPermittedFlagsNeverExpire();
}

// Ensures that every flag has an owner.
TEST(AboutFlagsTest, EveryFlagHasNonEmptyOwners) {
  flags_ui::testing::EnsureEveryFlagHasNonEmptyOwners();
}

// Ensures that owners conform to rules in flag-metadata.json.
TEST(AboutFlagsTest, OwnersLookValid) {
  flags_ui::testing::EnsureOwnersLookValid();
}

// For some bizarre reason, far too many people see a file filled with
// alphabetically-ordered items and think "hey, let me drop this new item into a
// random location!" Prohibit such behavior in the flags files.
TEST(AboutFlagsTest, FlagsListedInAlphabeticalOrder) {
  flags_ui::testing::EnsureFlagsAreListedInAlphabeticalOrder();
}

TEST(AboutFlagsTest, RecentUnexpireFlagsArePresent) {
  size_t count;
  const flags_ui::FeatureEntry* entries = testing::GetFeatureEntries(&count);
  flags_ui::testing::EnsureRecentUnexpireFlagsArePresent(
      base::make_span(entries, count), CHROME_VERSION_MAJOR);
}

class AboutFlagsHistogramTest : public ::testing::Test {
 protected:
  // This is a helper function to check that all IDs in enum LoginCustomFlags in
  // histograms.xml are unique.
  void SetSwitchToHistogramIdMapping(const std::string& switch_name,
                                     const Sample switch_histogram_id,
                                     std::map<std::string, Sample>* out_map) {
    const std::pair<std::map<std::string, Sample>::iterator, bool> status =
        out_map->insert(std::make_pair(switch_name, switch_histogram_id));
    if (!status.second) {
      EXPECT_TRUE(status.first->second == switch_histogram_id)
          << "Duplicate switch '" << switch_name
          << "' found in enum 'LoginCustomFlags' in histograms.xml.";
    }
  }

  // This method generates a hint for the user for what string should be added
  // to the enum LoginCustomFlags to make in consistent.
  std::string GetHistogramEnumEntryText(const std::string& switch_name,
                                        Sample value) {
    return base::StringPrintf(
        "<int value=\"%d\" label=\"%s\"/>", value, switch_name.c_str());
  }
};

TEST_F(AboutFlagsHistogramTest, CheckHistograms) {
  base::Optional<base::HistogramEnumEntryMap> login_custom_flags =
      base::ReadEnumFromEnumsXml("LoginCustomFlags");
  ASSERT_TRUE(login_custom_flags)
      << "Error reading enum 'LoginCustomFlags' from enums.xml.";

  // Build reverse map {switch_name => id} from login_custom_flags.
  SwitchToIdMap histograms_xml_switches_ids;

  EXPECT_TRUE(
      login_custom_flags->count(flags_ui::testing::kBadSwitchFormatHistogramId))
      << "Entry for UMA ID of incorrect command-line flag is not found in "
         "enums.xml enum LoginCustomFlags. "
         "Consider adding entry:\n"
      << "  " << GetHistogramEnumEntryText("BAD_FLAG_FORMAT", 0);
  // Check that all LoginCustomFlags entries have correct values.
  for (const auto& entry : *login_custom_flags) {
    if (entry.first == flags_ui::testing::kBadSwitchFormatHistogramId) {
      // Add error value with empty name.
      SetSwitchToHistogramIdMapping(std::string(), entry.first,
                                    &histograms_xml_switches_ids);
      continue;
    }
    const Sample uma_id = flags_ui::GetSwitchUMAId(entry.second);
    EXPECT_EQ(uma_id, entry.first)
        << "enums.xml enum LoginCustomFlags "
           "entry '"
        << entry.second << "' has incorrect value=" << entry.first << ", but "
        << uma_id << " is expected. Consider changing entry to:\n"
        << "  " << GetHistogramEnumEntryText(entry.second, uma_id);
    SetSwitchToHistogramIdMapping(entry.second, entry.first,
                                  &histograms_xml_switches_ids);
  }

  // Check that all flags in about_flags.cc have entries in login_custom_flags.
  std::set<std::string> all_flags = GetAllPublicSwitchesAndFeaturesForTesting();
  for (const std::string& flag : all_flags) {
    // Skip empty placeholders.
    if (flag.empty())
      continue;
    const Sample uma_id = flags_ui::GetSwitchUMAId(flag);
    EXPECT_NE(flags_ui::testing::kBadSwitchFormatHistogramId, uma_id)
        << "Command-line switch '" << flag
        << "' from about_flags.cc has UMA ID equal to reserved value "
           "kBadSwitchFormatHistogramId="
        << flags_ui::testing::kBadSwitchFormatHistogramId
        << ". Please modify switch name.";
    auto enum_entry = histograms_xml_switches_ids.lower_bound(flag);

    // Ignore case here when switch ID is incorrect - it has already been
    // reported in the previous loop.
    EXPECT_TRUE(enum_entry != histograms_xml_switches_ids.end() &&
                enum_entry->first == flag)
        << "enums.xml enum LoginCustomFlags doesn't contain switch '" << flag
        << "' (value=" << uma_id << " expected). Consider adding entry:\n"
        << "  " << GetHistogramEnumEntryText(flag, uma_id);
  }
}

}  // namespace about_flags
