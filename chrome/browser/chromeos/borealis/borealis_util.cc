// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_util.h"

#include "chrome/common/chrome_features.h"

namespace borealis {

const char kBorealisAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kBorealisDlcName[] = "borealis-dlc";

bool IsBorealisAllowed() {
  // Check that the Borealis feature is enabled.
  return base::FeatureList::IsEnabled(features::kBorealis);
}

}  // namespace borealis
