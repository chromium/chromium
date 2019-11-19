// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PERMISSIONS_UTIL_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PERMISSIONS_UTIL_H_

class Profile;

// Returns true if the user, as represented by |profile| is permitted to make
// calls to the remote Optimization Guide Service.
bool IsUserPermittedToFetchHints(Profile* profile);

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_PERMISSIONS_UTIL_H_
