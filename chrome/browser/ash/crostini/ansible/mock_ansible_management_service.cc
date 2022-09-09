// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/mock_ansible_management_service.h"

namespace crostini {
MockAnsibleManagementService::MockAnsibleManagementService(Profile* profile)
    : AnsibleManagementService(profile) {}
MockAnsibleManagementService::~MockAnsibleManagementService() = default;
}  // namespace crostini
