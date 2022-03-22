// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/system_display_ash.h"

#include <utility>

#include "ash/public/ash_interfaces.h"
#include "base/bind.h"
#include "chrome/browser/extensions/system_display/system_display_serialization.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace crosapi {

SystemDisplayAsh::SystemDisplayAsh() {
  observers_.set_disconnect_handler(
      base::BindRepeating(&SystemDisplayAsh::OnDisplayChangeObserverDisconnect,
                          weak_ptr_factory_.GetWeakPtr()));
}

SystemDisplayAsh::~SystemDisplayAsh() {
  StopDisplayChangedEventSources();
}

void SystemDisplayAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SystemDisplay> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SystemDisplayAsh::GetDisplayUnitInfoList(
    bool single_unified,
    GetDisplayUnitInfoListCallback callback) {
  extensions::DisplayInfoProvider::Get()->GetAllDisplaysInfo(
      single_unified,
      base::BindOnce(&SystemDisplayAsh::OnDisplayInfoResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemDisplayAsh::AddDisplayChangeObserver(
    mojo::PendingRemote<mojom::DisplayChangeObserver> observer) {
  if (observers_.empty())
    StartDisplayChangedEventSources();
  mojo::Remote<mojom::DisplayChangeObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

void SystemDisplayAsh::OnDisplayInfoResult(
    GetDisplayUnitInfoListCallback callback,
    std::vector<DisplayUnitInfo> src_info_list) {
  std::vector<crosapi::mojom::DisplayUnitInfoPtr> dst_info_list;
  dst_info_list.reserve(src_info_list.size());
  for (const auto& src_info : src_info_list) {
    dst_info_list.emplace_back(
        extensions::api::system_display::SerializeDisplayUnitInfo(src_info));
  }
  std::move(callback).Run(std::move(std::move(dst_info_list)));
}

void SystemDisplayAsh::OnDisplayChangeObserverDisconnect(
    mojo::RemoteSetElementId /*id*/) {
  if (observers_.empty())
    StopDisplayChangedEventSources();
}

void SystemDisplayAsh::DispatchCrosapiDisplayChangeObservers() {
  for (auto& observer : observers_) {
    observer->OnCrosapiDisplayChanged();
  }
}

void SystemDisplayAsh::OnDisplayAdded(const display::Display& new_display) {
  DispatchCrosapiDisplayChangeObservers();
}

void SystemDisplayAsh::OnDisplayRemoved(const display::Display& old_display) {
  DispatchCrosapiDisplayChangeObservers();
}

void SystemDisplayAsh::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t /*metrics*/) {
  DispatchCrosapiDisplayChangeObservers();
}

void SystemDisplayAsh::OnDisplayConfigChanged() {
  DispatchCrosapiDisplayChangeObservers();
}

void SystemDisplayAsh::StartDisplayChangedEventSources() {
  // Start Source 1.
  display_observer_.emplace(this);

  // Start Source 2.
  if (!is_observing_cros_display_config_) {
    mojo::PendingRemote<ash::mojom::CrosDisplayConfigController> display_config;
    ash::BindCrosDisplayConfigController(
        display_config.InitWithNewPipeAndPassReceiver());
    cros_display_config_ =
        mojo::Remote<ash::mojom::CrosDisplayConfigController>(
            std::move(display_config));
    mojo::PendingAssociatedRemote<ash::mojom::CrosDisplayConfigObserver>
        observer;
    cros_display_config_observer_receiver_.Bind(
        observer.InitWithNewEndpointAndPassReceiver());
    cros_display_config_->AddObserver(std::move(observer));
    is_observing_cros_display_config_ = true;
  }
}

void SystemDisplayAsh::StopDisplayChangedEventSources() {
  // Stop Source 2.
  if (is_observing_cros_display_config_) {
    is_observing_cros_display_config_ = false;
    cros_display_config_observer_receiver_.reset();
  }

  // Stop Source 1.
  display_observer_.reset();
}

}  // namespace crosapi
