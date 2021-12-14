// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_MOCK_DLP_CONTENT_MANAGER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_MOCK_DLP_CONTENT_MANAGER_ASH_H_

#include "base/callback_forward.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace policy {

class MockDlpContentManagerAsh : public DlpContentManagerAsh {
 public:
  MockDlpContentManagerAsh();
  ~MockDlpContentManagerAsh() override;

  MOCK_METHOD(void,
              OnConfidentialityChanged,
              (content::WebContents*, const DlpContentRestrictionSet&));
  MOCK_METHOD(void, OnWebContentsDestroyed, (content::WebContents*));
  MOCK_METHOD(DlpContentRestrictionSet,
              GetRestrictionSetForURL,
              (const GURL&),
              (const));
  MOCK_METHOD(void, OnVisibilityChanged, (content::WebContents*));
  MOCK_METHOD(bool,
              IsScreenshotApiRestricted,
              (const ScreenshotArea& area),
              (override));
  MOCK_METHOD(bool,
              IsScreenCaptureRestricted,
              (const content::DesktopMediaID& media_id),
              (override));
  MOCK_METHOD(void,
              CheckScreenShareRestriction,
              (const content::DesktopMediaID& media_id,
               const std::u16string& application_title,
               OnDlpRestrictionCheckedCallback callback),
              (override));

 protected:
  void Init() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_MOCK_DLP_CONTENT_MANAGER_ASH_H_
