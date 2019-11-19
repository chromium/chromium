// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_PROCESS_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_PROCESS_WIN_H_

#include <ostream>
#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"

class Profile;

namespace base {
class CommandLine;
}  // namespace base

namespace safe_browsing {

// Class that mocks the behaviour of the Chrome Cleaner process. Intended to be
// used in multi process tests. Example Usage:
//
// MULTIPROCESS_TEST_MAIN(MockChromeCleanerProcessMain) {
//   base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
//
//   MockChromeCleanerProcess mock_cleaner_process;
//   EXPECT_TRUE(mock_cleaner_process.InitWithCommandLine(*command_line));
//   if (::testing::Test::HasFailure())
//     return MockChromeCleanerProcess::kInternalTestFailureExitCode;
//
//   return mock_cleaner_process.Run();
// }
class MockChromeCleanerProcess {
 public:
  enum class CrashPoint {
    kNone,
    kOnStartup,
    kAfterConnection,
    kAfterRequestSent,
    kAfterResponseReceived,
    kNumCrashPoints,
  };

  // Indicates if a category of items (e.g. registry keys, extensions) to be
  // removed/changed will be sent from the cleaner process.
  enum class ItemsReporting {
    // Simulation of an older cleaner version that doesn't support sending
    // the category of items.
    kUnsupported,
    // Simulation of a cleaner version that supports sending the category of
    // items, but for which no items were reported.
    kNotReported,
    // The cleaner reported items to be removed/changed.
    kReported,
    kNumItemsReporting,
  };

  enum class UwsFoundStatus {
    kNoUwsFound,
    kUwsFoundRebootRequired,
    kUwsFoundNoRebootRequired,
  };

  enum class ExtensionCleaningFeatureStatus {
    kEnabled,
    kDisabled,
  };

  static constexpr int kInternalTestFailureExitCode = 100001;
  static constexpr int kDeliberateCrashExitCode = 100002;
  static constexpr int kNothingFoundExitCode = 2;
  static constexpr int kDeclinedExitCode = 44;
  static constexpr int kRebootRequiredExitCode = 15;
  static constexpr int kRebootNotRequiredExitCode = 0;

  static const base::char16 kInstalledExtensionId1[];
  static const base::char16 kInstalledExtensionName1[];
  static const base::char16 kInstalledExtensionId2[];
  static const base::char16 kInstalledExtensionName2[];
  static const base::char16 kUnknownExtensionId[];

  static void AddMockExtensionsToProfile(Profile* profile);

  class Options {
   public:
    static bool FromCommandLine(const base::CommandLine& command_line,
                                Options* options);

    Options();
    Options(const Options& other);
    Options& operator=(const Options& other);
    ~Options();

    void AddSwitchesToCommandLine(base::CommandLine* command_line) const;

    void SetReportedResults(bool has_files_to_remove,
                            ItemsReporting registry_keys_reporting,
                            ItemsReporting extensions_reporting);

    const std::vector<base::FilePath>& files_to_delete() const {
      return files_to_delete_;
    }
    const base::Optional<std::vector<base::string16>>& registry_keys() const {
      return registry_keys_;
    }
    const base::Optional<std::vector<base::string16>>& extension_ids() const {
      return extension_ids_;
    }

    const base::Optional<std::vector<base::string16>>&
    expected_extension_names() const {
      return expected_extension_names_;
    }

    void set_reboot_required(bool reboot_required) {
      reboot_required_ = reboot_required;
    }
    bool reboot_required() const { return reboot_required_; }

    void set_crash_point(CrashPoint crash_point) { crash_point_ = crash_point; }
    CrashPoint crash_point() const { return crash_point_; }

    void set_expected_user_response(
        chrome_cleaner::PromptUserResponse::PromptAcceptance
            expected_user_response) {
      expected_user_response_ = expected_user_response;
    }

    chrome_cleaner::PromptUserResponse::PromptAcceptance
    expected_user_response() const {
      return expected_user_response_;
    }

    ItemsReporting registry_keys_reporting() const {
      return registry_keys_reporting_;
    }

    ItemsReporting extensions_reporting() const {
      return extensions_reporting_;
    }

    int ExpectedExitCode(chrome_cleaner::PromptUserResponse::PromptAcceptance
                             received_prompt_acceptance) const;

   private:
    std::vector<base::FilePath> files_to_delete_;
    base::Optional<std::vector<base::string16>> registry_keys_;
    base::Optional<std::vector<base::string16>> extension_ids_;
    base::Optional<std::vector<base::string16>> expected_extension_names_;
    bool reboot_required_ = false;
    CrashPoint crash_point_ = CrashPoint::kNone;
    ItemsReporting registry_keys_reporting_ = ItemsReporting::kUnsupported;
    ItemsReporting extensions_reporting_ = ItemsReporting::kUnsupported;
    chrome_cleaner::PromptUserResponse::PromptAcceptance
        expected_user_response_ =
            chrome_cleaner::PromptUserResponse::UNSPECIFIED;
  };

  MockChromeCleanerProcess();
  ~MockChromeCleanerProcess();

  bool InitWithCommandLine(const base::CommandLine& command_line);

  // Call this in the main function of the mock Chrome Cleaner process. Returns
  // the exit code that should be used when the process exits.
  //
  // If a crash point has been specified in the options passed to the
  // constructor, the process will exit with a kDeliberateCrashExitCode exit
  // code, and this function will not return.
  int Run();

 private:
  Options options_;

  // Saved copy of command line for IPC setup.
  std::unique_ptr<base::CommandLine> command_line_;
};

// Making test parameter types printable.

std::ostream& operator<<(std::ostream& out,
                         MockChromeCleanerProcess::CrashPoint crash_point);

std::ostream& operator<<(std::ostream& out,
                         MockChromeCleanerProcess::UwsFoundStatus status);

std::ostream& operator<<(
    std::ostream& out,
    MockChromeCleanerProcess::ExtensionCleaningFeatureStatus status);

std::ostream& operator<<(
    std::ostream& out,
    MockChromeCleanerProcess::ItemsReporting items_reporting);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_PROCESS_WIN_H_
