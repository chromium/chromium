// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Params for the parameterized test IsGuestModeRequestedTest.
struct IsGuestModeRequestedTestParams {
  bool has_switch;
  bool pref_enforced;
  bool pref_enabled;
  bool expected_guest_mode_requested;
};

// clang-format off
const IsGuestModeRequestedTestParams kIsGuestModeRequestedParams[] {
  // has_switch | pref_enforced | pref_enabled | expected_guest_mode_requested
  {  true,        true,           true,          true},
  {  true,        true,           false,         false},
  {  true,        false,          true,          true},
  {  true,        false,          false,         false},
  {  false,       true,           true,          true},
  {  false,       true,           false,         false},
  {  false,       false,          true,          false},
  {  false,       false,          false,         false},
};
// clang-format on
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
class IsGuestModeRequestedTest
    : public testing::TestWithParam<IsGuestModeRequestedTestParams> {};

TEST_P(IsGuestModeRequestedTest, Requested) {
  TestingPrefServiceSimple local_state;
  local_state.registry()->RegisterBooleanPref(prefs::kBrowserGuestModeEnforced,
                                              false);
  local_state.registry()->RegisterBooleanPref(prefs::kBrowserGuestModeEnabled,
                                              false);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // Set parameters.
  IsGuestModeRequestedTestParams params = GetParam();
  local_state.SetBoolean(prefs::kBrowserGuestModeEnforced,
                         params.pref_enforced);
  local_state.SetBoolean(prefs::kBrowserGuestModeEnabled, params.pref_enabled);
  if (params.has_switch)
    command_line.AppendSwitch("guest");
  // Check expectation.
  EXPECT_EQ(params.expected_guest_mode_requested,
            profiles::IsGuestModeRequested(command_line, &local_state,
                                           /*show_warning=*/false));
}

INSTANTIATE_TEST_SUITE_P(ProfilesState,
                         IsGuestModeRequestedTest,
                         testing::ValuesIn(kIsGuestModeRequestedParams));
#endif  // !BUILDFLAG(IS_ANDROID)
