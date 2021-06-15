// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
#define ASH_CONTENT_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_

#include <string>

#include "ash/content/shimless_rma/mojom/shimless_rma.mojom.h"
#include "chromeos/dbus/rmad/rmad.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace shimless_rma {

class ShimlessRmaService : public mojom::ShimlessRmaService {
 public:
  ShimlessRmaService();
  ShimlessRmaService(const ShimlessRmaService&) = delete;
  ShimlessRmaService& operator=(const ShimlessRmaService&) = delete;

  ~ShimlessRmaService() override;

  void GetCurrentState(GetCurrentStateCallback callback) override;
  void GetNextState(GetNextStateCallback callback) override;
  void GetPrevState(GetPrevStateCallback callback) override;

  void AbortRma(AbortRmaCallback callback) override;

  void GetCurrentChromeVersion(
      GetCurrentChromeVersionCallback callback) override;
  void CheckForChromeUpdates(CheckForChromeUpdatesCallback callback) override;
  void UpdateChrome(UpdateChromeCallback callback) override;
  void UpdateChromeSkipped(UpdateChromeCallback callback) override;

  void SetSameOwner(SetSameOwnerCallback callback) override;
  void SetDifferentOwner(SetDifferentOwnerCallback callback) override;

  void ChooseManuallyDisableWriteProtect(
      ChooseManuallyDisableWriteProtectCallback callback) override;
  void ChooseRsuDisableWriteProtect(
      ChooseRsuDisableWriteProtectCallback callback) override;
  // TODO(gavindodd): GetRsuDisableChallengeCode()
  void SetRsuDisableWriteProtectCode(
      const std::string& code,
      SetRsuDisableWriteProtectCodeCallback callback) override;

  void GetComponentList(GetComponentListCallback callback) override;
  void SetComponentList(std::vector<mojom::ComponentPtr> component_list,
                        SetComponentListCallback callback) override;
  void ReworkMainboard(ReworkMainboardCallback callback) override;

  void ReimageRequired(ReimageRequiredCallback callback) override;
  void ReimageSkipped(ReimageSkippedCallback callback) override;
  void ReimageFromDownload(ReimageFromDownloadCallback callback) override;
  void ReimageFromUsb(ReimageFromUsbCallback callback) override;
  void GetRegionList(GetRegionListCallback callback) override;

  void GetSkuList(GetSkuListCallback callback) override;
  void GetOriginalSerialNumber(
      GetOriginalSerialNumberCallback callback) override;
  void GetSerialNumber(GetSerialNumberCallback callback) override;
  void SetSerialNumber(const std::string& serial_number,
                       SetSerialNumberCallback callback) override;
  void GetOriginalRegion(GetOriginalRegionCallback callback) override;
  void GetRegion(GetRegionCallback callback) override;
  void SetRegion(int8_t region_index, SetRegionCallback callback) override;
  void GetOriginalSku(GetOriginalSkuCallback callback) override;
  void GetSku(GetSkuCallback callback) override;
  void SetSku(int8_t sku_index, SetSkuCallback callback) override;

  void FinalizeAndReboot(CutoffBatteryCallback callback) override;
  void FinalizeAndShutdown(CutoffBatteryCallback callback) override;
  void CutoffBattery(CutoffBatteryCallback callback) override;

  void ObserveError(
      ::mojo::PendingRemote<mojom::ErrorObserver> observer) override;
  void ObserveCalibrationProgress(
      ::mojo::PendingRemote<mojom::CalibrationObserver> observer) override;
  void ObserveProvisioningProgress(
      ::mojo::PendingRemote<mojom::ProvisioningObserver> observer) override;
  void ObserveHardwareWriteProtectionState(
      ::mojo::PendingRemote<mojom::HardwareWriteProtectionStateObserver>
          observer) override;
  void ObservePowerCableState(
      ::mojo::PendingRemote<mojom::PowerCableStateObserver> observer) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::ShimlessRmaService> pending_receiver);

 private:
  mojo::Receiver<mojom::ShimlessRmaService> receiver_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShimlessRmaService> weak_ptr_factory_{this};
};
}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_CONTENT_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_SERVICE_H_
