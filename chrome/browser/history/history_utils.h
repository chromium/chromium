// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_UTILS_H_
#define CHROME_BROWSER_HISTORY_HISTORY_UTILS_H_

class GURL;

// Returns true if this looks like the type of URL that should be added to the
// history. This filters out URLs such a JavaScript.
bool CanAddURLToHistory(const GURL& url);

#endif  // CHROME_BROWSER_HISTORY_HISTORY_UTILS_H_
