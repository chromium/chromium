// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_TEST_SCOPED_MAHI_WEB_CONTENTS_MANAGER_FOR_TESTING_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_TEST_SCOPED_MAHI_WEB_CONTENTS_MANAGER_FOR_TESTING_H_

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

namespace mahi {

// Helper class to automatically set and reset the `MahiWebContentsManager`
// instance.
// The caller (i.e. test) should manage the lifetime of
// `mahi_web_content_manager_for_testing`. This class does not own it.
class ScopedMahiWebContentsManagerForTesting {
 public:
  explicit ScopedMahiWebContentsManagerForTesting(
      MahiWebContentsManager* mahi_web_content_manager_for_testing);
  ~ScopedMahiWebContentsManagerForTesting();
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_TEST_SCOPED_MAHI_WEB_CONTENTS_MANAGER_FOR_TESTING_H_
