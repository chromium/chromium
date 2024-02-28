// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/enterprise_domain_model.h"

#include "ash/public/cpp/login_types.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"

namespace ash {

EnterpriseDomainModel::EnterpriseDomainModel() = default;

EnterpriseDomainModel::~EnterpriseDomainModel() = default;

void EnterpriseDomainModel::AddObserver(EnterpriseDomainObserver* observer) {
  observers_.AddObserver(observer);
}

void EnterpriseDomainModel::RemoveObserver(EnterpriseDomainObserver* observer) {
  observers_.RemoveObserver(observer);
}

void EnterpriseDomainModel::SetDeviceEnterpriseInfo(
    const DeviceEnterpriseInfo& device_enterprise_info) {
  device_enterprise_info_ = device_enterprise_info;
  for (auto& observer : observers_)
    observer.OnDeviceEnterpriseInfoChanged();
}

void EnterpriseDomainModel::SetEnterpriseAccountDomainInfo(
    const std::string& account_domain_manager) {
  account_domain_manager_ = account_domain_manager;
  for (auto& observer : observers_)
    observer.OnEnterpriseAccountDomainChanged();
}

}  // namespace ash
