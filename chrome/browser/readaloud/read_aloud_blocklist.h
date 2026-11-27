// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_BLOCKLIST_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_BLOCKLIST_H_

#include "base/feature.h"

class GURL;

namespace readaloud {

// Enabled-by-default kill switch for the Read Aloud domain blocklist.
BASE_DECLARE_FEATURE(kReadAloudBlockedDomains);

// Manages the domain blocklist for Read Aloud, suppressing the entrypoint and
// playback on non-article web apps and private portals.
class ReadAloudBlocklist {
 public:
  // Public constructor allows unit tests to instantiate isolated instances.
  // Production callers should use GetInstance().
  ReadAloudBlocklist();
  ReadAloudBlocklist(const ReadAloudBlocklist&) = delete;
  ReadAloudBlocklist& operator=(const ReadAloudBlocklist&) = delete;
  ~ReadAloudBlocklist();

  static ReadAloudBlocklist& GetInstance();

  // Returns true if `url` is blocked from Read Aloud.
  bool IsBlocked(const GURL& url) const;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_BLOCKLIST_H_
