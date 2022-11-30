// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"

#include "chrome/browser/ash/plugin_vm/fake_plugin_vm_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

TEST(PluginVmFeaturesTest, TestFakeReplaces) {
  PluginVmFeatures* original = PluginVmFeatures::Get();
  {
    FakePluginVmFeatures plugin_vm_features;
    EXPECT_NE(original, PluginVmFeatures::Get());
    EXPECT_EQ(&plugin_vm_features, PluginVmFeatures::Get());
  }
  EXPECT_EQ(original, PluginVmFeatures::Get());
}

}  // namespace plugin_vm
