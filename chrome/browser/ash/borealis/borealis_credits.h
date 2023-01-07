// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CREDITS_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CREDITS_H_

#include <string>

#include "base/functional/callback_forward.h"

class Profile;

namespace borealis {

// Loads the borealis credits file from the DLC, invoking |callback| with the
// HTML contents in a string. If the credits can not be loaded, the empty string
// will be used instead.
void LoadBorealisCredits(Profile* profile,
                         base::OnceCallback<void(std::string)> callback);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CREDITS_H_
