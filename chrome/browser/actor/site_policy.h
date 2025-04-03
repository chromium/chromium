// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SITE_POLICY_H_
#define CHROME_BROWSER_ACTOR_SITE_POLICY_H_

#include "base/functional/callback_forward.h"

namespace tabs {
class TabInterface;
}

class Profile;

namespace actor {

// Called during initialization of the given profile, to load the blocklist.
void InitActionBlocklist(Profile* profile);

using DecisionCallback = base::OnceCallback<void(/*may_act=*/bool)>;

// Checks whether the actor may perform actions on the given tab based on the
// last committed document and URL. Invokes the callback with true if it is
// allowed.
void MayActOnTab(const tabs::TabInterface& tab, DecisionCallback callback);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SITE_POLICY_H_
