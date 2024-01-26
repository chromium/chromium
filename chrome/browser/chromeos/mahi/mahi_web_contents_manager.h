// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_

#include "base/no_destructor.h"

namespace content {
class WebContents;
}  // namespace content

namespace mahi {

class ScopedMahiWebContentsManagerForTesting;
class MockMahiWebContentsManager;

// `MahiWebContentsManager` is the central class for mahi web contents in the
// browser (ash and lacros) responsible for:
// 1. Being the single source of truth for mahi browser parameters like focused
//    web page, the lasted extracted web page, etc. and providing this
//    information to ChromeOS backend as needed.
// 2. Decides the distillability of a web page and extract contents from a
//    requested web page.
class MahiWebContentsManager {
 public:
  MahiWebContentsManager(const MahiWebContentsManager&) = delete;
  MahiWebContentsManager& operator=(const MahiWebContentsManager&) = delete;

  static MahiWebContentsManager* Get();

  // Called when the focused tab changed.
  // Virtual so we can override in tests.
  virtual void OnFocusChanged(content::WebContents* web_contents);

  // Called when the focused tab finish loading.
  // Virtual so we can override in tests.
  virtual void OnFocusedPageLoadComplete(content::WebContents* web_contents);

 private:
  friend base::NoDestructor<MahiWebContentsManager>;
  // Friends to access some test-only functions.
  friend class ScopedMahiWebContentsManagerForTesting;
  friend class MockMahiWebContentsManager;

  static void SetInstanceForTesting(MahiWebContentsManager* manager);
  static void ResetInstanceForTesting();

  MahiWebContentsManager();
  virtual ~MahiWebContentsManager();
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_WEB_CONTENTS_MANAGER_H_
