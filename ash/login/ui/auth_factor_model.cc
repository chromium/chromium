// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/auth_icon_view.h"

namespace ash {

AuthFactorModel::AuthFactorModel() = default;

AuthFactorModel::~AuthFactorModel() = default;

void AuthFactorModel::Init(AuthIconView* icon,
                           base::RepeatingClosure on_state_changed_callback) {
  DCHECK(!icon_) << "Init should only be called once.";
  icon_ = icon;
  on_state_changed_callback_ = on_state_changed_callback;
}

void AuthFactorModel::SetVisible(bool visible) {
  DCHECK(icon_);
  icon_->SetVisible(visible);
}

void AuthFactorModel::OnThemeChanged() {
  if (icon_)
    UpdateIcon(icon_);
}

void AuthFactorModel::NotifyOnStateChanged() {
  DCHECK(icon_);
  if (on_state_changed_callback_) {
    on_state_changed_callback_.Run();
  }
  UpdateIcon(icon_);
}

}  // namespace ash
