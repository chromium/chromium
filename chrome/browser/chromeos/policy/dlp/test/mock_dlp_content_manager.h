// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_MOCK_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_MOCK_DLP_CONTENT_MANAGER_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockDlpContentManager : public DlpContentManager {
 public:
  MockDlpContentManager();
  ~MockDlpContentManager() override;

  MOCK_METHOD(void,
              OnConfidentialityChanged,
              (content::WebContents*, const DlpContentRestrictionSet&),
              (override));
  MOCK_METHOD(void,
              OnWebContentsDestroyed,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void, OnVisibilityChanged, (content::WebContents*), (override));
  MOCK_METHOD(bool,
              IsScreenshotApiRestricted,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void,
              CheckScreenShareRestriction,
              (const content::DesktopMediaID&,
               const std::u16string&,
               WarningCallback),
              (override));
  MOCK_METHOD(void,
              OnScreenShareStarted,
              (const std::string&,
               std::vector<content::DesktopMediaID>,
               const std::u16string&,
               base::RepeatingClosure,
               content::MediaStreamUI::StateChangeCallback,
               content::MediaStreamUI::SourceCallback),
              (override));
  MOCK_METHOD(void,
              OnScreenShareStopped,
              (const std::string&, const content::DesktopMediaID&),
              (override));
  MOCK_METHOD(ConfidentialContentsInfo,
              GetScreenShareConfidentialContentsInfo,
              (const content::DesktopMediaID&, content::WebContents*),
              (const override));
  MOCK_METHOD(void,
              TabLocationMaybeChanged,
              (content::WebContents * web_contents),
              (override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_MOCK_DLP_CONTENT_MANAGER_H_
