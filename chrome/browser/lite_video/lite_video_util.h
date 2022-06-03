// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_UTIL_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_UTIL_H_

class Profile;

// Returns true if the user, as represented by |profile| is allowed to have
// the LiteVideo optimization applied.
bool IsLiteVideoAllowedForUser(Profile* profile);

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_UTIL_H_
