// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PERSONALIZATION_APP_ENTERPRISE_POLICY_DELEGATE_H_
#define ASH_PUBLIC_CPP_PERSONALIZATION_APP_ENTERPRISE_POLICY_DELEGATE_H_

#include "base/observer_list_types.h"

namespace ash::personalization_app {

// Delegate for checking and observing enterprise policy status of
// personalization features.
class EnterprisePolicyDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnUserImageIsEnterpriseManagedChanged(
        bool is_enterprise_managed) = 0;

    // This may be called even when the enterprise state for a given user has
    // not changed, if another user on the same device just changed wallpaper.
    virtual void OnWallpaperIsEnterpriseManagedChanged(
        bool is_enterprise_managed) = 0;
  };

  virtual ~EnterprisePolicyDelegate() = default;

  virtual bool IsUserImageEnterpriseManaged() const = 0;
  virtual bool IsWallpaperEnterpriseManaged() const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace ash::personalization_app

#endif  // ASH_PUBLIC_CPP_PERSONALIZATION_APP_ENTERPRISE_POLICY_DELEGATE_H_
