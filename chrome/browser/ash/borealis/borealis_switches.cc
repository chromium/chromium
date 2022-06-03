// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_switches.h"

namespace borealis {
namespace switches {

// Stores additional options to be used when launching borealis. Format should
// be "-<switch>=<value>;..."". Switches and what they do are documented in
// chrome/browser/ash/borealis/borealis_launch_options.h
const char kLaunchOptions[] = "borealis-launch-options";

}  // namespace switches
}  // namespace borealis
