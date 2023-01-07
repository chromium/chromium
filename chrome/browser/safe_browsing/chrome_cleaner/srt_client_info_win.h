// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CLIENT_INFO_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CLIENT_INFO_WIN_H_

namespace safe_browsing {

// Encodes Chrome's channel as an integer to be passed to the SRT on the command
// line. The SRT binary expects to receive Chrome's channel encoded as:
//     0: unknown; 1: canary; 2: dev; 3: beta; 4: stable.
int ChannelAsInt();

// Returns true if there is a profile in which the user has opted into Safe
// Browsing extended reporting.
bool SafeBrowsingExtendedReportingEnabled();

// Returns true if there is a profile in which the user has opted into Safe
// Browsing extended reporting with reporting level SBER_LEVEL_SCOUT.
bool SafeBrowsingExtendedReportingScoutEnabled();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CLIENT_INFO_WIN_H_
