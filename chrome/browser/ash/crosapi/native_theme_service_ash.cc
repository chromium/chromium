// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"

#include "ui/native_theme/native_theme.h"

namespace crosapi {

/******** NativeThemeServiceAsh::Dispatcher ********/

NativeThemeServiceAsh::Dispatcher::Dispatcher() {
  ui::NativeTheme::GetInstanceForNativeUi()->AddObserver(this);
}

NativeThemeServiceAsh::Dispatcher::~Dispatcher() {
  ui::NativeTheme::GetInstanceForNativeUi()->RemoveObserver(this);
}

void NativeThemeServiceAsh::Dispatcher::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  DCHECK_EQ(ui::NativeTheme::GetInstanceForNativeUi(), observed_theme);

  mojom::NativeThemeInfoPtr info = NativeThemeServiceAsh::GetNativeThemeInfo();
  for (auto& observer : observers_) {
    mojom::NativeThemeInfoPtr info_copy = info->Clone();
    observer->OnNativeThemeInfoChanged(std::move(info_copy));
  }
}

/******** NativeThemeServiceAsh ********/

// static
mojom::NativeThemeInfoPtr NativeThemeServiceAsh::GetNativeThemeInfo() {
  auto info = mojom::NativeThemeInfo::New();
  info->dark_mode =
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors();
  return info;
}

NativeThemeServiceAsh::NativeThemeServiceAsh() = default;

NativeThemeServiceAsh::~NativeThemeServiceAsh() = default;

void NativeThemeServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NativeThemeService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NativeThemeServiceAsh::AddNativeThemeInfoObserver(
    mojo::PendingRemote<mojom::NativeThemeInfoObserver> observer) {
  // Fire the observer with the initial value.
  mojo::Remote<mojom::NativeThemeInfoObserver> remote(std::move(observer));
  remote->OnNativeThemeInfoChanged(GetNativeThemeInfo());

  dispatcher_.observers_.Add(std::move(remote));
}

}  // namespace crosapi
