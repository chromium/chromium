// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_APPS_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_APPS_H_

#include <string>

class Profile;

namespace borealis {

// Create a borealis app for use in testing, registered with the given
// |profile|, its name in the VM would have been "|desktop_file_id|.desktop.
void CreateFakeApp(Profile* profile, std::string desktop_file_id);

// Creates borealis' main app for use in testing. The app will be registered
// with the given |profile|.
void CreateFakeMainApp(Profile* profile);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_APPS_H_
