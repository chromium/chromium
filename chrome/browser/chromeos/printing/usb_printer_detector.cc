// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/usb_printer_detector.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/usb_printer_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace {

// Given a usb device, guesses the make and model for a driver lookup.
//
// TODO(https://crbug.com/895037): Possibly go deeper and query the IEEE1284
// fields for make and model if we determine those are more likely to contain
// what we want.  Strings currently come from udev.
// TODO(https://crbug.com/895037): When above is added, parse out document
// formats and add to DetectedPrinter
std::string GuessEffectiveMakeAndModel(
    const device::mojom::UsbDeviceInfo& device) {
  return base::UTF16ToUTF8(GetManufacturerName(device)) + " " +
         base::UTF16ToUTF8(GetProductName(device));
}

// The PrinterDetector that drives the flow for setting up a USB printer to use
// CUPS backend.
class UsbPrinterDetectorImpl : public UsbPrinterDetector,
                               public device::mojom::UsbDeviceManagerClient {
 public:
  explicit UsbPrinterDetectorImpl(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager)
      : device_manager_(std::move(device_manager)) {
    device_manager_.set_disconnect_handler(
        base::BindOnce(&UsbPrinterDetectorImpl::OnDeviceManagerConnectionError,
                       weak_factory_.GetWeakPtr()));

    // Listen for added/removed device events.
    device_manager_->EnumerateDevicesAndSetClient(
        client_receiver_.BindNewEndpointAndPassRemote(),
        base::BindOnce(&UsbPrinterDetectorImpl::OnGetDevices,
                       weak_factory_.GetWeakPtr()));
  }

  ~UsbPrinterDetectorImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
  }

  // PrinterDetector override.
  void RegisterPrintersFoundCallback(OnPrintersFoundCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    DCHECK(!on_printers_found_callback_);
    on_printers_found_callback_ = std::move(cb);
  }

  // PrinterDetector override.
  std::vector<DetectedPrinter> GetPrinters() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    std::vector<DetectedPrinter> printers_list;
    printers_list.reserve(printers_.size());
    for (const auto& entry : printers_) {
      printers_list.push_back(entry.second);
    }
    return printers_list;
  }

 private:
  // Callback for initial enumeration of usb devices.
  void OnGetDevices(std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const auto& device : devices) {
      DoAddDevice(*device);
    }
  }

  void OnDeviceManagerConnectionError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    device_manager_.reset();
    client_receiver_.reset();
    printers_.clear();
  }

  void DoAddDevice(const device::mojom::UsbDeviceInfo& device_info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!UsbDeviceIsPrinter(device_info)) {
      return;
    }

    std::unique_ptr<Printer> converted = UsbDeviceToPrinter(device_info);
    if (!converted.get()) {
      // An error will already have been logged if we failed to convert.
      return;
    }
    DetectedPrinter entry;
    entry.printer = *converted;
    entry.ppd_search_data.usb_vendor_id = device_info.vendor_id;
    entry.ppd_search_data.usb_product_id = device_info.product_id;
    entry.ppd_search_data.make_and_model.push_back(
        GuessEffectiveMakeAndModel(device_info));
    entry.ppd_search_data.discovery_type =
        PrinterSearchData::PrinterDiscoveryType::kUsb;

    // Query printer for an IEEE Device ID.
    mojo::Remote<device::mojom::UsbDevice> device;
    device_manager_->GetDevice(device_info.guid,
                               device.BindNewPipeAndPassReceiver(),
                               mojo::NullRemote() /* device_client */);
    GetDeviceId(std::move(device),
                base::BindOnce(&UsbPrinterDetectorImpl::OnGetDeviceId,
                               weak_factory_.GetWeakPtr(), std::move(entry),
                               device_info.guid));
  }

  void OnGetDeviceId(DetectedPrinter entry,
                     std::string guid,
                     UsbPrinterId printer_id) {
    entry.ppd_search_data.printer_id = std::move(printer_id);

    // Add detected printer.
    printers_[guid] = entry;
    if (on_printers_found_callback_) {
      on_printers_found_callback_.Run(GetPrinters());
    }
  }

  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    DCHECK(device_info);
    DoAddDevice(*device_info);
  }

  // device::mojom::UsbDeviceManagerClient implementation.
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    DCHECK(device_info);
    if (!UsbDeviceIsPrinter(*device_info)) {
      return;
    }
    printers_.erase(device_info->guid);
    if (on_printers_found_callback_) {
      on_printers_found_callback_.Run(GetPrinters());
    }
  }

  SEQUENCE_CHECKER(sequence_);

  // Map from USB GUID to DetectedPrinter for all detected printers.
  std::map<std::string, DetectedPrinter> printers_;

  OnPrintersFoundCallback on_printers_found_callback_;

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};
  base::WeakPtrFactory<UsbPrinterDetectorImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<UsbPrinterDetector> UsbPrinterDetector::Create() {
  // Bind to the DeviceService for USB device manager.
  mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
  content::GetSystemConnector()->Connect(
      device::mojom::kServiceName,
      usb_manager.InitWithNewPipeAndPassReceiver());
  return std::make_unique<UsbPrinterDetectorImpl>(std::move(usb_manager));
}

std::unique_ptr<UsbPrinterDetector> UsbPrinterDetector::CreateForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager) {
  return std::make_unique<UsbPrinterDetectorImpl>(std::move(usb_manager));
}

}  // namespace chromeos
