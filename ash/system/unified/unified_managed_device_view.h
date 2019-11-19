// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_MANAGED_DEVICE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_MANAGED_DEVICE_VIEW_H_

#include "ash/session/session_observer.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ImageView;
}  // namespace views

namespace ash {

// Row in the unified system tray bubble shown when the device is currently
// managed by an administrator (by a domain admin or FamilyLink).
class ASH_EXPORT UnifiedManagedDeviceView : public views::View,
                                            public SessionObserver,
                                            public EnterpriseDomainObserver {
 public:
  UnifiedManagedDeviceView();
  ~UnifiedManagedDeviceView() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // EnterpriseDomainObserver:
  void OnEnterpriseDomainChanged() override;

  // views::TrayItemView:
  const char* GetClassName() const override;

 private:
  void Update();

  // Owned by views hierarchy.
  views::ImageView* const icon_;
  views::Label* const label_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedManagedDeviceView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_MANAGED_DEVICE_VIEW_H_
