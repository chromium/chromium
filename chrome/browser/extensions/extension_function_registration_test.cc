// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/extension_browsertest.h"
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
  std::set<functions::HistogramValue> seen_histograms;
  for (const auto& key_value : factories) {
    const ExtensionFunctionRegistry::FactoryEntry& entry = key_value.second;
    SCOPED_TRACE(entry.function_name_);
    EXPECT_TRUE(seen_names.insert(entry.function_name_).second);
    // NOTE: We explicitly don't check the factory here. On certain platforms
    // with enough compiler optimization, the templated factories are re-used
    // for different functions.
    // EXPECT_TRUE(seen_factories.insert(entry.factory_).second);

    // The chrome.test API uses an "unknown" histogram value, but should be the
    // only API that does.
    if (entry.histogram_value_ == functions::UNKNOWN) {
      EXPECT_TRUE(base::StartsWith(entry.function_name_, "test.",
                                   base::CompareCase::SENSITIVE));
    } else {
      EXPECT_TRUE(seen_histograms.insert(entry.histogram_value_).second);
    }
  }
}

}  // namespace extensions
