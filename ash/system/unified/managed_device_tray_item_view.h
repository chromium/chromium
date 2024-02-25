// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_MANAGED_DEVICE_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_UNIFIED_MANAGED_DEVICE_TRAY_ITEM_VIEW_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/enterprise/enterprise_domain_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// A view to show an icon in the status tray when the device is managed by
// an organization admin. Observes login status in order to show/hide the
// icon reflecting the latest status.
class ManagedDeviceTrayItemView : public TrayItemView,
                                  public SessionObserver,
                                  public EnterpriseDomainObserver {
  METADATA_HEADER(ManagedDeviceTrayItemView, TrayItemView)

 public:
  explicit ManagedDeviceTrayItemView(Shelf* shelf);

  ManagedDeviceTrayItemView(const ManagedDeviceTrayItemView&) = delete;
  ManagedDeviceTrayItemView& operator=(const ManagedDeviceTrayItemView&) =
      delete;

  ~ManagedDeviceTrayItemView() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // EnterpriseDomainObserver:
  void OnDeviceEnterpriseInfoChanged() override;
  void OnEnterpriseAccountDomainChanged() override;

  // views::TrayItemView:
  void OnThemeChanged() override;
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;

 private:
  void Update();
  // Gets the icon based on user session state.
  const gfx::VectorIcon* GetIcon();
  void UpdateIcon();
  void UpdateTooltipText();
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_MANAGED_DEVICE_TRAY_ITEM_VIEW_H_
