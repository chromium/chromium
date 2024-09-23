// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PUBLIC_ACCOUNT_MONITORING_INFO_DIALOG_H_
#define ASH_LOGIN_UI_PUBLIC_ACCOUNT_MONITORING_INFO_DIALOG_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

class LoginExpandedPublicAccountView;

// Dialog displayed after selecting the public session learn more button,
// present on the public account expanded view.
class ASH_EXPORT PublicAccountMonitoringInfoDialog
    : public views::DialogDelegateView {
  METADATA_HEADER(PublicAccountMonitoringInfoDialog, views::DialogDelegateView)

 public:
  explicit PublicAccountMonitoringInfoDialog(
      base::WeakPtr<LoginExpandedPublicAccountView> controller);
  PublicAccountMonitoringInfoDialog(const PublicAccountMonitoringInfoDialog&) =
      delete;
  PublicAccountMonitoringInfoDialog& operator=(
      const PublicAccountMonitoringInfoDialog&) = delete;
  ~PublicAccountMonitoringInfoDialog() override;

  bool IsVisible();
  void Show();

  // views::DialogDelegate:
  void AddedToWidget() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  base::WeakPtr<LoginExpandedPublicAccountView> controller_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PUBLIC_ACCOUNT_MONITORING_INFO_DIALOG_H_
