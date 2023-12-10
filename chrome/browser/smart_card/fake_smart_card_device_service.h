// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_FAKE_SMART_CARD_DEVICE_SERVICE_H_
#define CHROME_BROWSER_SMART_CARD_FAKE_SMART_CARD_DEVICE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/smart_card.mojom.h"

class FakeSmartCardDeviceService
    : public KeyedService,
      public device::mojom::SmartCardContextFactory,
      public device::mojom::SmartCardContext {
 public:
  FakeSmartCardDeviceService();
  ~FakeSmartCardDeviceService() override;

  // device::mojom::SmartCardContextFactory overrides:
  void CreateContext(CreateContextCallback) override;

  mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory();

 private:
  struct PendingStatusChange;
  struct ReaderState;

  // device::mojom::SmartCardContext overrides:
  void ListReaders(ListReadersCallback callback) override;
  void GetStatusChange(
      base::TimeDelta timeout,
      std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
      GetStatusChangeCallback callback) override;
  void Cancel(CancelCallback callback) override;
  void Connect(const std::string& reader,
               device::mojom::SmartCardShareMode share_mode,
               device::mojom::SmartCardProtocolsPtr preferred_protocols,
               ConnectCallback callback) override;

  void TryResolvePendingStatusChanges();
  bool TryResolve(PendingStatusChange& pending_status_change);
  static void FillStateOut(
      device::mojom::SmartCardReaderStateOut& state_out,
      const device::mojom::SmartCardReaderStateIn& state_in,
      const ReaderState& reader_state);

  mojo::ReceiverSet<device::mojom::SmartCardContextFactory>
      context_factory_receivers_;

  mojo::ReceiverSet<device::mojom::SmartCardContext> context_receivers_;

  base::flat_map<std::string, ReaderState> readers_;

  std::vector<std::unique_ptr<PendingStatusChange>> pending_status_changes_;
};

#endif  // CHROME_BROWSER_SMART_CARD_FAKE_SMART_CARD_DEVICE_SERVICE_H_
