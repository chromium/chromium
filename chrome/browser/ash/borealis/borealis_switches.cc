// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_switches.h"

namespace borealis {
namespace switches {

// Allows passing a BorealisLaunchOptions string to the chrome process, which
// will be stored in the kExtraLaunchOptions. For the format, see the
// documentation in chrome/browser/ash/borealis/borealis_launch_options.h.
const char kLaunchOptions[] = "borealis-launch-options";

}  // namespace switches
}  // namespace borealis
