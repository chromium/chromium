// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_switches.h"

namespace actor::switches {

// Bypasses several of the actor's safety checks, such as requiring SafeBrowsing
// to be enabled and the blocklist.
// Note that some checks, like requiring https, are not affected by this.
const char kDisableActorSafetyChecks[] = "disable-actor-safety-checks";

}  // namespace actor::switches
