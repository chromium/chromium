// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/dev_mode_bubble_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"

namespace extensions {

using DevModeBubbleDelegateUiUnitTest = ExtensionServiceTestBase;

TEST_F(DevModeBubbleDelegateUiUnitTest, Test) {
  FeatureSwitch::ScopedOverride dev_mode_highlighting(
      FeatureSwitch::force_dev_mode_highlighting(), true);

  InitializeEmptyExtensionService();

  scoped_refptr<const Extension> extension = ExtensionBuilder("test").Build();
  service()->AddExtension(extension.get());

  DevModeBubbleDelegate bubble_delegate(profile());
  EXPECT_TRUE(bubble_delegate.ShouldIncludeExtension(extension.get()));

  EXPECT_TRUE(bubble_delegate.GetDismissButtonLabel().empty());
}

}  // namespace extensions
