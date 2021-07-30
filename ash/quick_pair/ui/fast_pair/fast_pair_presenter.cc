// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"

#include <string>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_image_decoder.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

FastPairPresenter::FastPairPresenter()
    : image_decoder_(std::make_unique<FastPairImageDecoder>(
          std::unique_ptr<image_fetcher::ImageFetcher>())) {}

FastPairPresenter::~FastPairPresenter() = default;

void FastPairPresenter::ShowDiscovery(scoped_refptr<Device> device,
                                      DiscoveryCallback callback) {
  FastPairRepository::Get()->GetDeviceMetadata(
      device->metadata_id,
      base::BindOnce(&FastPairPresenter::ShowDiscoveryMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device),
                     std::move(callback)));
}

void FastPairPresenter::ShowDiscoveryMetadataRetrieved(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    absl::optional<nearby::fastpair::GetObservedDeviceResponse>
        device_metadata) {
  QP_LOG(VERBOSE) << __func__;
  if (!device_metadata) {
    return;
  }
  const std::string& string_data = device_metadata->image();
  std::vector<uint8_t> binary_data(string_data.begin(), string_data.end());
  image_decoder_->DecodeImage(
      binary_data,
      base::BindOnce(&FastPairPresenter::ShowDiscoveryImpl,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device),
                     std::move(callback), *device_metadata));
}
void FastPairPresenter::ShowDiscoveryImpl(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    nearby::fastpair::GetObservedDeviceResponse device_metadata,
    gfx::Image image) {
  QP_LOG(VERBOSE) << __func__;
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  notification_controller_->ShowDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata.device().name()), std::move(image),
      base::BindOnce(&FastPairPresenter::OnDiscoveryClicked,
                     weak_pointer_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&FastPairPresenter::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void FastPairPresenter::OnDiscoveryClicked(DiscoveryCallback callback) {
  std::move(callback).Run(DiscoveryAction::kPairToDevice);
}

void FastPairPresenter::OnDiscoveryDismissed(DiscoveryCallback callback,
                                             bool user_dismissed) {
  std::move(callback).Run(user_dismissed ? DiscoveryAction::kDismissedByUser
                                         : DiscoveryAction::kDismissed);
}

void FastPairPresenter::ShowPairing(scoped_refptr<Device> device) {
  notification_controller_->ShowPairingNotification(
      base::ASCIIToUTF16(device->metadata_id), gfx::Image(), base::DoNothing(),
      base::DoNothing());
}

void FastPairPresenter::ShowPairingFailed(scoped_refptr<Device> device,
                                          PairingFailedCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  notification_controller_->ShowErrorNotification(
      base::ASCIIToUTF16(device->metadata_id), gfx::Image(),
      base::BindOnce(&FastPairPresenter::OnNavigateToSettings,
                     weak_pointer_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&FastPairPresenter::OnPairingFailedDismissed,
                     weak_pointer_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void FastPairPresenter::OnNavigateToSettings(PairingFailedCallback callback) {
  std::move(callback).Run(PairingFailedAction::kNavigateToSettings);
}

void FastPairPresenter::OnPairingFailedDismissed(PairingFailedCallback callback,
                                                 bool user_dimissed) {
  std::move(callback).Run(user_dimissed ? PairingFailedAction::kDismissedByUser
                                        : PairingFailedAction::kDismissed);
}

void FastPairPresenter::ShowAssociateAccount(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback) {}

void FastPairPresenter::ShowCompanionApp(scoped_refptr<Device> device,
                                         CompanionAppCallback callback) {}

}  // namespace quick_pair
}  // namespace ash
