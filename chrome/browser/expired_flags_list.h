// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXPIRED_FLAGS_LIST_H_
#define CHROME_BROWSER_EXPIRED_FLAGS_LIST_H_

// This header file declares a data structure that is generated at compile time
// by //tools/flags/generate_expired_list.py - also see the
// //chrome/browser:expired_flags_list target.

namespace flags {

struct ExpiredFlag {
  const char* name;
  int mstone;
};

// This array of names is terminated with a flag whose name is nullptr.
extern const ExpiredFlag kExpiredFlags[];

}  // namespace flags

#endif  // CHROME_BROWSER_EXPIRED_FLAGS_LIST_H_
