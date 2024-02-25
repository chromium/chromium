// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/test/scoped_mahi_web_contents_manager_for_testing.h"

namespace mahi {

ScopedMahiWebContentsManagerForTesting::ScopedMahiWebContentsManagerForTesting(
    MahiWebContentsManager* test_manager) {
  MahiWebContentsManager::SetInstanceForTesting(test_manager);
}

ScopedMahiWebContentsManagerForTesting::
    ~ScopedMahiWebContentsManagerForTesting() {
  MahiWebContentsManager::ResetInstanceForTesting();
}

}  // namespace mahi
