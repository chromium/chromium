// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_COMMON_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_COMMON_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}

namespace test {

// Reference file locations.

// Checks if the user has the reference files directory, returns true if so.
// If the user's checkout don't have these dirs, they need to configure their
// .gclient as described in chrome/test/data/webrtc/resources/README. The reason
// for this is that we don't want to burden regular chrome devs with downloading
// these sizable reference files by default.
bool HasReferenceFilesInCheckout();

// Verifies both the YUV and Y4M version of the reference file exists.
bool HasYuvAndY4mFile(const base::FilePath::CharType* reference_file);

// Retrieves the reference files dir, to which file names can be appended.
base::FilePath GetReferenceFilesDir();

// Retrieves a tool binary path from chrome/test/data/webrtc/resources/tools,
// according to platform. If we're running on Linux, requesting pesq will yield
// chrome/test/data/webrtc/resources/tools/linux/pesq, whereas the same call on
// Windows will yield chrome/test/data/webrtc/resources/tools/win/pesq.exe.
// This function does not check the binary actually exists.
base::FilePath GetToolForPlatform(const std::string& tool_name);

extern const base::FilePath::CharType kReferenceFileName360p[];
extern const base::FilePath::CharType kReferenceFileName720p[];
extern const base::FilePath::CharType kYuvFileExtension[];
extern const base::FilePath::CharType kY4mFileExtension[];
extern const char kAdviseOnGclientSolution[];

// Executes javascript code which will sleep for |timeout_msec| milliseconds.
// Returns true on success.
bool SleepInJavascript(content::WebContents* tab_contents, int timeout_msec);

// This function will execute the provided |javascript| until the script's
// completion value is |evaluates_to|. Returns false if we exceed the
// TestTimeouts::action_max_timeout().
// TODO(phoglund): Consider a better interaction method with the javascript
// than polling javascript methods.
bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents);
bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents,
                      int poll_interval_msec);

// This function will execute the provided |closure| until it evaluates true,
// causing a function return value of true, unless we exceed the
// TestTimeouts::action_max_timeout() in which case the function returns false.
bool PollingWaitUntilClosureEvaluatesTrue(
    base::RepeatingCallback<bool()> closure,
    content::WebContents* tab_contents,
    base::TimeDelta poll_interval);

}  // namespace test

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_BROWSERTEST_COMMON_H_
