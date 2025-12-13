// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/test/test_support_jni_headers/ScopedFeatureListTestUtils_jni.h"

namespace base::android {

// Use a ScopedFeatureList to update the feature states with the values in the
// command line.
// Background information: Each test class and test method can
// override the feature value via the @EnableFeatures and @DisableFeatures
// annotations. These annotations are picked up by the the test fixture and the
// test fixture then appends the appropriate flags to the command line.
// Afterwards, this function needs to be called to update the feature states
// with the values in the command line.
// This function is expected to be called multiple times when a test run
// involves multiple tests. Internally, this manages a single ScopedFeatureList,
// which is never destroyed. Calling this function subsequent times will reset
// that instance to the new state.
static void JNI_ScopedFeatureListTestUtils_InitScopedFeatureList(JNIEnv* env) {
  static base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Reset();
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string enabled =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  scoped_feature_list.InitFromCommandLine(enabled, disabled);
}

}  // namespace base::android

DEFINE_JNI(ScopedFeatureListTestUtils)
