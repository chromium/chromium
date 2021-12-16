// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_CONTENT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_CONTENT_MANAGER_H_

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
  MOCK_METHOD(void,
              CheckScreenShareRestriction,
              (const content::DesktopMediaID& media_id,
               const std::u16string& application_title,
               OnDlpRestrictionCheckedCallback callback),
              (override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_MOCK_DLP_CONTENT_MANAGER_H_
