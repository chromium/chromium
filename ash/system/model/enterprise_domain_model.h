// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_ENTERPRISE_DOMAIN_MODEL_H_
#define ASH_SYSTEM_MODEL_ENTERPRISE_DOMAIN_MODEL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/login_types.h"
#include "base/observer_list.h"

namespace ash {

class EnterpriseDomainObserver;

// Model to store enterprise enrollment state.
class ASH_EXPORT EnterpriseDomainModel {
 public:
  EnterpriseDomainModel();

  EnterpriseDomainModel(const EnterpriseDomainModel&) = delete;
  EnterpriseDomainModel& operator=(const EnterpriseDomainModel&) = delete;

  ~EnterpriseDomainModel();

  void AddObserver(EnterpriseDomainObserver* observer);
  void RemoveObserver(EnterpriseDomainObserver* observer);

  void SetDeviceEnterpriseInfo(
      const DeviceEnterpriseInfo& device_enterprise_info);

  // |account_domain_manager| should be either an empty string, a domain name
  // (foo.com) or an email address (user@foo.com). This string will be displayed
  // to the user without modification.
  void SetEnterpriseAccountDomainInfo(
      const std::string& account_domain_manager);

  const std::string& enterprise_domain_manager() const {
    return device_enterprise_info_.enterprise_domain_manager;
  }

  ManagementDeviceMode management_device_mode() const {
    return device_enterprise_info_.management_device_mode;
  }

  const std::string& account_domain_manager() const {
    return account_domain_manager_;
  }

 private:
  DeviceEnterpriseInfo device_enterprise_info_;
  std::string account_domain_manager_;

  base::ObserverList<EnterpriseDomainObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_ENTERPRISE_DOMAIN_MODEL_H_
