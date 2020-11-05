// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_MANAGED_DEVICE_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_MANAGED_DEVICE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ImageView;
}  // namespace views

namespace ash {

// Row in the unified system tray bubble shown when the device is currently
// managed by an administrator (by a domain admin or FamilyLink).
class ASH_EXPORT UnifiedManagedDeviceView : public views::Button,
                                            public SessionObserver,
                                            public EnterpriseDomainObserver {
 public:
  explicit UnifiedManagedDeviceView(UnifiedSystemTrayController* controller);
  ~UnifiedManagedDeviceView() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // EnterpriseDomainObserver:
  void OnEnterpriseDomainChanged() override;

  // views::Button:
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  void Update();

  // Owned by views hierarchy.
  views::ImageView* const icon_;
  views::Label* const label_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedManagedDeviceView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_MANAGED_DEVICE_VIEW_H_
