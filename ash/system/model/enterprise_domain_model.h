// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_ENTERPRISE_DOMAIN_MODEL_H_
#define ASH_SYSTEM_MODEL_ENTERPRISE_DOMAIN_MODEL_H_

#include <memory>

#include "ash/ash_export.h"
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

  // |enterprise_domain_manager| and |account_domain_manager| should be either
  // an empty string, a domain name (foo.com) or an email address
  // (user@foo.com). This string will be displayed to the user without
  // modification.
  void SetEnterpriseDomainInfo(const std::string& enterprise_domain_manager,
                               bool active_directory_managed);
  void SetEnterpriseAccountDomainInfo(
      const std::string& account_domain_manager);

  const std::string& enterprise_domain_manager() const {
    return enterprise_domain_manager_;
  }
  bool active_directory_managed() const { return active_directory_managed_; }
  const std::string& account_domain_manager() const {
    return account_domain_manager_;
  }

 private:
  // The name of the entity that manages the device and current account user.
  //    For standard Dasher domains, this will be the domain name (foo.com).
  //    For FlexOrgs, this will be the admin's email (user@foo.com).
  //    For Active Directory or not enteprise enrolled, this will be an empty
  //    string.
  std::string enterprise_domain_manager_;
  std::string account_domain_manager_;

  // Whether this is an Active Directory managed enterprise device.
  bool active_directory_managed_ = false;

  base::ObserverList<EnterpriseDomainObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_ENTERPRISE_DOMAIN_MODEL_H_
