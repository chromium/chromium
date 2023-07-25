// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_APPS_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_APPS_H_

#include <string>

class Profile;

namespace borealis {

// Create a borealis app for use in testing, for an app defined in a file
// called "|desktop_file_id|.desktop" with an Exec key having value |exec|.
void CreateFakeApp(Profile* profile,
                   std::string desktop_file_id,
                   std::string exec);

// The App List Id of an app created using CreateFakeApp().
std::string FakeAppId(const std::string& desktop_file_id);

// Creates borealis' main app for use in testing. The app will be registered
// with the given |profile|.
void CreateFakeMainApp(Profile* profile);

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_APPS_H_
