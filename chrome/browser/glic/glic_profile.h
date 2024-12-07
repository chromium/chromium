// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_H_

class Profile;

// TODO(https://crbug.com/379165457): Hang this off of GlobalFeatures once there
// is state to make sure that it is appropriately scoped.

// The profile to open the glic UI with. This will be the last profile used
// with glic, but if that doesn't exist, fallback to the pinned glic profile,
// and failing that, the last active profile.
Profile* LastActiveGlicProfile();

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_H_
