// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/annotations/annotation_control_provider.h"

#include "base/containers/flat_map.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using ::testing::Key;
using ::testing::UnorderedElementsAre;

TEST(AnnotationControlProviderTest, ProvidesRequiredControls) {
  AnnotationControlProvider provider;

  const base::flat_map<std::string, AnnotationControl> actual_controls =
      provider.GetControls();
  EXPECT_FALSE(actual_controls.empty());

  // Verify that the map contains the expected hash codes.
  EXPECT_THAT(
      actual_controls,
      UnorderedElementsAre(Key("88863520"), Key("104798869"), Key("86429515"),
                           Key("134729048"), Key("28498700"), Key("108804096"),
                           Key("46208118"), Key("99742369"), Key("4306022"),
                           Key("108903331"), Key("50127013"), Key("24186190"),
                           Key("62443804")));
}

}  // namespace policy
