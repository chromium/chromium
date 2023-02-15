// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_FUCHSIA_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_FUCHSIA_H_

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include <string>

namespace policy {

// Implementation of BrowserDMTokenStorage delegate for Fuchsia.
class BrowserDMTokenStorageFuchsia : public BrowserDMTokenStorage::Delegate {
 public:
  BrowserDMTokenStorageFuchsia();
  BrowserDMTokenStorageFuchsia(const BrowserDMTokenStorageFuchsia&) = delete;
  BrowserDMTokenStorageFuchsia& operator=(const BrowserDMTokenStorageFuchsia&) =
      delete;
  ~BrowserDMTokenStorageFuchsia() override;

 private:
  // override BrowserDMTokenStorage::Delegate
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  bool CanInitEnrollmentToken() const override;
  BrowserDMTokenStorage::StoreTask SaveDMTokenTask(
      const std::string& token,
      const std::string& client_id) override;
  BrowserDMTokenStorage::StoreTask DeleteDMTokenTask(
      const std::string& client_id) override;
  scoped_refptr<base::TaskRunner> SaveDMTokenTaskRunner() override;

  scoped_refptr<base::TaskRunner> task_runner_;
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_FUCHSIA_H_
