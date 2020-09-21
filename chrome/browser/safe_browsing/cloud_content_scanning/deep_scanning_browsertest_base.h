// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_BROWSERTEST_BASE_H_

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

// Base test class for deep scanning browser tests. Common utility functions
// used by browser tests should be added to this class.
class DeepScanningBrowserTestBase : public InProcessBrowserTest {
 public:
  DeepScanningBrowserTestBase();
  ~DeepScanningBrowserTestBase() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Setters for deep scanning policies.
  void SetUnsafeEventsReportingPolicy(bool report);

  // Sets up a FakeDeepScanningDialogDelegate to use this class's StatusCallback
  // and EncryptionStatusCallback. Also sets up a test DM token.
  void SetUpDelegate();

  // Sets up a FakeDeepScanningDialogDelegate that never returns responses.
  void SetUpUnresponsiveDelegate();

  // Set up a quit closure to be called by the test. This is useful to control
  // when the test ends.
  void SetQuitClosure(base::RepeatingClosure quit_closure);
  void CallQuitClosure();

  // Set what StatusCallback returns.
  void SetStatusCallbackResponse(
      enterprise_connectors::ContentAnalysisResponse response);

  // Callbacks used to set up the fake delegate factory.
  enterprise_connectors::ContentAnalysisResponse StatusCallback(
      const base::FilePath& path);
  bool EncryptionStatusCallback(const base::FilePath& path);

  // Creates temporary files for testing in |temp_dir_|, and add them to |data|.
  void CreateFilesForTest(const std::vector<std::string>& paths,
                          const std::vector<std::string>& contents,
                          DeepScanningDialogDelegate::Data* data);

  const std::vector<base::FilePath>& created_file_paths() const;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::RepeatingClosure quit_closure_;
  enterprise_connectors::ContentAnalysisResponse
      connector_status_callback_response_;
  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> created_file_paths_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_BROWSERTEST_BASE_H_
