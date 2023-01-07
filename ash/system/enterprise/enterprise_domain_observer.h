// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ENTERPRISE_ENTERPRISE_DOMAIN_OBSERVER_H_
#define ASH_SYSTEM_ENTERPRISE_ENTERPRISE_DOMAIN_OBSERVER_H_

namespace ash {

class EnterpriseDomainObserver {
 public:
  virtual ~EnterpriseDomainObserver() {}

  virtual void OnDeviceEnterpriseInfoChanged() = 0;

  virtual void OnEnterpriseAccountDomainChanged() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ENTERPRISE_ENTERPRISE_DOMAIN_OBSERVER_H_
