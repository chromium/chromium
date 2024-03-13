// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

using ExtensionFunctionRegistrationTest = ExtensionBrowserTest;

// Test that all functions are registered with unique names, histogram values,
// and factories. This is a browser test (rather than a unit test) to (help)
// ensure that all the optional factories and services are indeed instantiated.
IN_PROC_BROWSER_TEST_F(ExtensionFunctionRegistrationTest,
                       CheckForDuplicateEntries) {
  // Verify the ExtensionSystem is ready (and thus all extension functions
  // registered) before checking.
  base::RunLoop run_loop;
  ExtensionSystem::Get(profile())->ready().Post(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();

  const ExtensionFunctionRegistry::FactoryMap& factories =
      ExtensionFunctionRegistry::GetInstance().GetFactoriesForTesting();
  // Sanity check: Many, many functions should have been registered.
  EXPECT_GT(factories.size(), 500u);

  std::set<std::string> seen_names;
  std::map<functions::HistogramValue, std::string> seen_histograms;

  // The following are methods that are undocumented and may or may not ship
  // with a final API. We allow them to use the UNKNOWN histogram entry in the
  // meantime.
  // Each entry should have a bug number associated with it.
  static const constexpr char* kAllowedUnknownHistogramEntries[] = {
      // https://crbug.com/1339382.
      "offscreen.hasDocument",
  };

  for (const auto& key_value : factories) {
    const ExtensionFunctionRegistry::FactoryEntry& entry = key_value.second;
    SCOPED_TRACE(entry.function_name_);
    EXPECT_TRUE(seen_names.insert(entry.function_name_).second);
    // NOTE: We explicitly don't check the factory here. On certain platforms
    // with enough compiler optimization, the templated factories are re-used
    // for different functions.
    // EXPECT_TRUE(seen_factories.insert(entry.factory_).second);

    if (entry.histogram_value_ == functions::UNKNOWN) {
      // The chrome.test API uses UNKNOWN; it's only used in tests.
      if (base::StartsWith(entry.function_name_, "test.",
                           base::CompareCase::SENSITIVE)) {
        continue;
      }
      // Some undocumented, unlaunched APIs may also use UNKNOWN if it's unclear
      // (or unlikely) if they will ever launch.
      if (base::Contains(kAllowedUnknownHistogramEntries,
                         std::string(entry.function_name_))) {
        continue;
      }
      ADD_FAILURE() << "Un-allowlisted API found using UNKNOWN histogram entry."
                    << entry.function_name_;
    } else {
      bool is_success =
          seen_histograms.emplace(entry.histogram_value_, entry.function_name_)
              .second;
      if (!is_success) {
        ADD_FAILURE() << "Histogram " << entry.function_name_ << " with value "
                      << entry.histogram_value_
                      << " already exists with another name - "
                      << seen_histograms.find(entry.histogram_value_)->second;
      }
    }
  }
}

}  // namespace extensions
