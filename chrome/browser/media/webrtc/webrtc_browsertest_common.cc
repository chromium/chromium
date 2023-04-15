// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"

#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test_utils.h"

namespace test {

// Relative to the chrome/test/data directory.
const base::FilePath::CharType kReferenceFilesDirName[] =
    FILE_PATH_LITERAL("webrtc/resources");
const base::FilePath::CharType kReferenceFileName360p[] =
    FILE_PATH_LITERAL("reference_video_640x360_30fps");
const base::FilePath::CharType kReferenceFileName720p[] =
    FILE_PATH_LITERAL("reference_video_1280x720_30fps");
const base::FilePath::CharType kYuvFileExtension[] = FILE_PATH_LITERAL("yuv");
const base::FilePath::CharType kY4mFileExtension[] = FILE_PATH_LITERAL("y4m");

// This message describes how to modify your .gclient to get the reference
// video files downloaded for you.
const char kAdviseOnGclientSolution[] =
    "To run this test, you must run download_from_google_storage --config\n"
    "and follow the instructions (use 'browser' for project id)\n"
    "You also need to add this solution to your .gclient:\n"
    "{\n"
    "  \"name\"        : \"webrtc.DEPS\",\n"
    "  \"url\"         : \"https://chromium.googlesource.com/chromium/deps/"
    "webrtc/webrtc.DEPS\",\n"
    "}\n"
    "and run gclient sync. This will download the required ref files.";

#if defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
#if BUILDFLAG(IS_CHROMEOS_ASH)
const int kDefaultPollIntervalMsec = 2000;
#else
const int kDefaultPollIntervalMsec = 1000;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#else
#if BUILDFLAG(IS_CHROMEOS_ASH)
const int kDefaultPollIntervalMsec = 500;
#else
const int kDefaultPollIntervalMsec = 250;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif

bool IsErrorResult(const std::string& result) {
  return base::StartsWith(result, "failed-",
                          base::CompareCase::INSENSITIVE_ASCII);
}

base::FilePath GetReferenceFilesDir() {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);

  return test_data_dir.Append(kReferenceFilesDirName);
}

base::FilePath GetToolForPlatform(const std::string& tool_name) {
  base::FilePath tools_dir =
      GetReferenceFilesDir().Append(FILE_PATH_LITERAL("tools"));
#if BUILDFLAG(IS_WIN)
  return tools_dir
      .Append(FILE_PATH_LITERAL("win"))
      .AppendASCII(tool_name)
      .AddExtension(FILE_PATH_LITERAL("exe"));
#elif BUILDFLAG(IS_MAC)
  return tools_dir.Append(FILE_PATH_LITERAL("mac")).AppendASCII(tool_name);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return tools_dir.Append(FILE_PATH_LITERAL("linux")).AppendASCII(tool_name);
#else
  CHECK(false) << "Can't retrieve tool " << tool_name << " on this platform.";
  return base::FilePath();
#endif
}

bool HasReferenceFilesInCheckout() {
  if (!base::PathExists(GetReferenceFilesDir())) {
    LOG(ERROR)
        << "Cannot find the working directory for the reference video "
        << "files, expected at " << GetReferenceFilesDir().value() << ". " <<
        kAdviseOnGclientSolution;
    return false;
  }
  return HasYuvAndY4mFile(test::kReferenceFileName360p) &&
      HasYuvAndY4mFile(test::kReferenceFileName720p);
}

bool HasYuvAndY4mFile(const base::FilePath::CharType* reference_file) {
  base::FilePath webrtc_reference_video_yuv = GetReferenceFilesDir()
      .Append(reference_file).AddExtension(kYuvFileExtension);
  if (!base::PathExists(webrtc_reference_video_yuv)) {
    LOG(ERROR)
        << "Missing YUV reference video to be used for quality"
        << " comparison, expected at " << webrtc_reference_video_yuv.value()
        << ". " << kAdviseOnGclientSolution;
    return false;
  }

  base::FilePath webrtc_reference_video_y4m = GetReferenceFilesDir()
      .Append(reference_file).AddExtension(kY4mFileExtension);
  if (!base::PathExists(webrtc_reference_video_y4m)) {
    LOG(ERROR)
        << "Missing Y4M reference video to be used for quality"
        << " comparison, expected at "<< webrtc_reference_video_y4m.value()
        << ". " << kAdviseOnGclientSolution;
    return false;
  }
  return true;
}

bool SleepInJavascript(content::WebContents* tab_contents, int timeout_msec) {
  const std::string javascript = base::StringPrintf(
      "new Promise(resolve => {"
      "  setTimeout(function() {"
      "    resolve('sleep-ok');"
      "  }, %d);"
      "});",
      timeout_msec);

  return content::EvalJs(tab_contents, javascript).ExtractString() ==
         "sleep-ok";
}

bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents) {
  return PollingWaitUntil(javascript, evaluates_to, tab_contents,
                          kDefaultPollIntervalMsec);
}

bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents,
                      int poll_interval_msec) {
  base::Time start_time = base::Time::Now();
  base::TimeDelta timeout = TestTimeouts::action_max_timeout();
  std::string result;

  while (base::Time::Now() - start_time < timeout) {
    result = content::EvalJs(tab_contents, javascript).ExtractString();

    if (evaluates_to == result) {
      return true;
    } else if (IsErrorResult(result)) {
      LOG(ERROR) << "|" << javascript << "| returned an error: " << result;
      return false;
    }

    // Sleep a bit here to keep this loop from spinlocking too badly.
    if (!SleepInJavascript(tab_contents, poll_interval_msec)) {
      // TODO(phoglund): Figure out why this fails every now and then.
      // It's not a huge deal if it does though.
      LOG(ERROR) << "Failed to sleep.";
    }
  }
  LOG(ERROR)
      << "Timed out while waiting for " << javascript
      << " to evaluate to " << evaluates_to << "; last result was '" << result
      << "'";
  return false;
}

bool PollingWaitUntilClosureEvaluatesTrue(
    base::RepeatingCallback<bool()> closure,
    content::WebContents* tab_contents,
    base::TimeDelta poll_interval) {
  base::Time start_time = base::Time::Now();
  base::TimeDelta timeout = TestTimeouts::action_max_timeout();
  while (base::Time::Now() - start_time < timeout) {
    if (closure.Run())
      return true;
    // Sleep a bit here to keep this loop from spinlocking too badly.
    if (!SleepInJavascript(tab_contents, poll_interval.InMilliseconds())) {
      LOG(ERROR) << "Failed to sleep.";
    }
  }
  LOG(ERROR) << "Timed out while waiting for closure to evaluate true";
  return false;
}

}  // namespace test
