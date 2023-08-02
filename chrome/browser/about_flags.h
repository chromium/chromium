// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ABOUT_FLAGS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_base.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"

class Profile;

namespace base {
class FeatureList;
}

namespace flags_ui {
class FlagsStorage;
}

namespace about_flags {

// This method returns the FlagsStorage instance to use for this platform. In
// addition, this returns the access level for the flags. The callback may be
// synchronously invoked.
// Note that |profile| is only used in ash-chrome.
using GetStorageCallback =
    base::OnceCallback<void(std::unique_ptr<flags_ui::FlagsStorage> storage,
                            flags_ui::FlagAccess access)>;
void GetStorage(Profile* profile, GetStorageCallback callback);

// Returns true if the FeatureEntry should not be shown.
bool ShouldSkipConditionalFeatureEntry(const flags_ui::FlagsStorage* storage,
                                       const flags_ui::FeatureEntry& entry);

// Reads the state from |flags_storage| and adds the command line flags
// belonging to the active feature entries to |command_line|.
void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            flags_ui::SentinelsMode sentinels);

// Registers variations parameter values selected for features in about:flags.
// The selected flags are retrieved from |flags_storage|, the registered
// variation parameters are connected to their corresponding features in
// |feature_list|. Returns the (possibly empty) list of additional variation ids
// to register in the MetricsService that come from variations selected using
// chrome://flags.
std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list);

// Gets the list of feature entries. Entries that are available for the current
// platform are appended to |supported_entries|; all other entries are appended
// to |unsupported_entries|.
void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::Value::List& supported_entries,
                           base::Value::List& unsupported_entries);

// Gets the list of feature entries for the deprecated flags page. Entries that
// are available for the current platform are appended to |supported_entries|;
// all other entries are appended to |unsupported_entries|.
void GetFlagFeatureEntriesForDeprecatedPage(
    flags_ui::FlagsStorage* flags_storage,
    flags_ui::FlagAccess access,
    base::Value::List& supported_entries,
    base::Value::List& unsupported_entries);

// Gets the FlagsState used in about_flags.
flags_ui::FlagsState* GetCurrentFlagsState();

// Returns true if one of the feature entry flags has been flipped since
// startup.
bool IsRestartNeededToCommitChanges();

// Enables or disables the current with id |internal_name|.
void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable);

// Sets a flag value with a list of origins given by |value|. Origins in |value|
// can be separated by a comma or whitespace. Invalid URLs will be dropped when
// setting the command line flag.
// E.g. SetOriginListFlag("test-flag",
//                        "http://example.test1 http://example.test2",
//                        flags_storage);
// will add --test-flag=http://example.test to the command line.
void SetOriginListFlag(const std::string& internal_name,
                       const std::string& value,
                       flags_ui::FlagsStorage* flags_storage);

// Sets a flag value with a string given by |value|.
void SetStringFlag(const std::string& internal_name,
                   const std::string& value,
                   flags_ui::FlagsStorage* flags_storage);

// Removes all switches that were added to a command line by a previous call to
// |ConvertFlagsToSwitches()|.
void RemoveFlagsSwitches(base::CommandLine::SwitchMap* switch_list);

// Reset all flags to the default state by clearing all flags.
void ResetAllFlags(flags_ui::FlagsStorage* flags_storage);

#if BUILDFLAG(IS_CHROMEOS)
// Show flags of the other browser (Lacros/Ash).
void CrosUrlFlagsRedirect();
#endif

// Sends UMA stats about experimental flag usage. This should be called once per
// startup.
void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage,
                         const std::string& histogram_name);

namespace testing {

// This class sets the testing feature entries to the feature entries passed in
// to Init. It clears the testing feature entries on destruction, so
// the feature entries return to their non test values.
class ScopedFeatureEntries final {
 public:
  explicit ScopedFeatureEntries(
      const std::vector<flags_ui::FeatureEntry>& entries);
  ~ScopedFeatureEntries();
};

base::span<const flags_ui::FeatureEntry> GetFeatureEntries();

}  // namespace testing

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
