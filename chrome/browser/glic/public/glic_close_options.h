// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_CLOSE_OPTIONS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_CLOSE_OPTIONS_H_

namespace glic {

struct CloseOptions {
  bool suppress_animations = false;
  // True when the user closed the panel directly (close button, Esc), as
  // opposed to Chrome closing it, e.g. on a conversation switch.
  bool user_initiated = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_CLOSE_OPTIONS_H_
