// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_FAKE_BROWSER_DM_TOKEN_STORAGE_H_
#define CHROME_BROWSER_POLICY_FAKE_BROWSER_DM_TOKEN_STORAGE_H_

#include "chrome/browser/policy/browser_dm_token_storage.h"

namespace policy {

// A fake BrowserDMTokenStorage implementation for testing. Test
// cases can set CBCM related values instead of reading it
// from operating system.
class FakeBrowserDMTokenStorage : public BrowserDMTokenStorage {
 public:
  FakeBrowserDMTokenStorage();
  FakeBrowserDMTokenStorage(const std::string& client_id,
                            const std::string& enrollment_token,
                            const std::string& dm_token,
                            bool enrollment_error_option);
  ~FakeBrowserDMTokenStorage() override;

  void SetClientId(const std::string& client_id);
  void SetEnrollmentToken(const std::string& enrollment_token);
  void SetDMToken(const std::string& dm_token);
  void SetEnrollmentErrorOption(bool option);
  // Determines if SaveDMToken will be succeeded or not.
  void EnableStorage(bool storage_enabled);

  // policy::BrowserDMTokenStorage:
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  void SaveDMToken(const std::string& token) override;

 private:
  std::string client_id_ = "";
  std::string enrollment_token_ = "";
  std::string dm_token_ = "";
  bool enrollment_error_option_ = true;

  bool storage_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeBrowserDMTokenStorage);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_FAKE_BROWSER_DM_TOKEN_STORAGE_H_
