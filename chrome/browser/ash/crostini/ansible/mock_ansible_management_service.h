// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_MOCK_ANSIBLE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_MOCK_ANSIBLE_MANAGEMENT_SERVICE_H_

#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace crostini {
class MockAnsibleManagementService : public AnsibleManagementService {
 public:
  static std::unique_ptr<KeyedService> Build(Profile* profile) {
    return std::make_unique<MockAnsibleManagementService>(profile);
  }

  explicit MockAnsibleManagementService(Profile* profile);
  ~MockAnsibleManagementService() override;

  MOCK_METHOD(void,
              ConfigureContainer,
              (const guest_os::GuestId& container_id,
               base::FilePath playbook,
               base::OnceCallback<void(bool success)> callback),
              (override));
};
}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_MOCK_ANSIBLE_MANAGEMENT_SERVICE_H_
