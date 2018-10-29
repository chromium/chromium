// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/feature_toggler.h"

FeatureToggler::FeatureToggler(const base::Feature& feature) {
  if (GetParam()) {
    override_features_.InitAndEnableFeature(feature);
  } else {
    override_features_.InitAndDisableFeature(feature);
  }
}

FeatureToggler::~FeatureToggler() {}
