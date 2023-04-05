// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_MSBB_DELEGATE_H_
#define CHROME_BROWSER_COMPANION_CORE_MSBB_DELEGATE_H_

namespace companion {

// Helper class to read and write the setting for make searches and browsing
// better.
class MsbbDelegate {
 public:
  virtual ~MsbbDelegate() = default;

  // Enable the setting for make searches and browsing better.
  virtual void EnableMsbb(bool enable_msbb) = 0;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_MSBB_DELEGATE_H_
