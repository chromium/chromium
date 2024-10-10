// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_detector.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ash/printing/printer_event_tracker_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/printing/usb_printer_util.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/usb_printer_id.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace ash {
namespace {

// Helper class, redirect calls to SessionController provided as a constructor's
// parameter. If the parameter is nullptr, the pointer returned by
// ash::SessionController::Get() is used (if not nullptr).
class SessionControllerWrapper {
 public:
  explicit SessionControllerWrapper(SessionController* session_controller)
      : session_controller_(session_controller) {}
  SessionControllerWrapper(const SessionControllerWrapper&) = delete;
  SessionControllerWrapper& operator=(const SessionControllerWrapper&) = delete;

  void AddObserver(SessionObserver* observer) const {
    if (SessionController* controller = GetSessionController(); controller) {
      controller->AddObserver(observer);
    }
  }
  void RemoveObserver(SessionObserver* observer) const {
    if (SessionController* controller = GetSessionController(); controller) {
      controller->RemoveObserver(observer);
    }
  }
  bool IsScreenLocked() const {
    if (SessionController* controller = GetSessionController(); controller) {
      return controller->IsScreenLocked();
    }
    return false;
  }

 private:
  SessionController* GetSessionController() const {
    if (session_controller_) {
      return session_controller_;
    }
    return SessionController::Get();
  }
  raw_ptr<SessionController> session_controller_;
};

// The PrinterDetector that drives the flow for setting up a USB printer to use
// CUPS backend.
class UsbPrinterDetectorImpl : public UsbPrinterDetector,
                               public device::mojom::UsbDeviceManagerClient,
                               public ash::SessionObserver {
 public:
  explicit UsbPrinterDetectorImpl(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager,
      ash::SessionController* session_controller = nullptr)
      : device_manager_(std::move(device_manager)),
        session_controller_(session_controller) {
    device_manager_.set_disconnect_handler(
        base::BindOnce(&UsbPrinterDetectorImpl::OnDeviceManagerConnectionError,
                       weak_factory_.GetWeakPtr()));

    // Listen for added/removed device events.
    device_manager_->EnumerateDevicesAndSetClient(
        client_receiver_.BindNewEndpointAndPassRemote(),
        base::BindOnce(&UsbPrinterDetectorImpl::OnGetDevices,
                       weak_factory_.GetWeakPtr()));

    session_controller_.AddObserver(this);
  }

  ~UsbPrinterDetectorImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    session_controller_.RemoveObserver(this);
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
    printers_list.reserve(printers_ready_.size());
    for (const auto& entry : printers_ready_) {
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
    printers_ready_.clear();
    printers_locked_screen_.clear();
  }

  void DoAddDevice(const device::mojom::UsbDeviceInfo& device_info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!UsbDeviceIsPrinter(device_info)) {
      return;
    }

    DetectedPrinter entry;
    if (!UsbDeviceToPrinter(device_info, &entry)) {
      // An error will already have been logged if we failed to convert.
      PRINTER_LOG(EVENT) << "USB printer "
                         << base::StringPrintf("%04x:%04x",
                                               device_info.vendor_id,
                                               device_info.product_id)
                         << " was detected but not recognized";
      return;
    }
    std::string make_and_model = GuessEffectiveMakeAndModel(device_info);
    PRINTER_LOG(EVENT) << "USB printer "
                       << base::StringPrintf("%04x:%04x", device_info.vendor_id,
                                             device_info.product_id)
                       << " was detected: " << make_and_model;

    entry.ppd_search_data.usb_vendor_id = device_info.vendor_id;
    entry.ppd_search_data.usb_product_id = device_info.product_id;
    entry.ppd_search_data.make_and_model.push_back(std::move(make_and_model));
    entry.ppd_search_data.discovery_type =
        chromeos::PrinterSearchData::PrinterDiscoveryType::kUsb;

    // Query printer for an IEEE Device ID.
    mojo::Remote<device::mojom::UsbDevice> device;
    device_manager_->GetDevice(device_info.guid,
                               /*blocked_interface_classes=*/{},
                               device.BindNewPipeAndPassReceiver(),
                               /*device_client=*/mojo::NullRemote());
    GetDeviceId(std::move(device),
                base::BindOnce(&UsbPrinterDetectorImpl::OnGetDeviceId,
                               weak_factory_.GetWeakPtr(), std::move(entry),
                               device_info.guid));
  }

  void OnGetDeviceId(DetectedPrinter entry,
                     std::string guid,
                     chromeos::UsbPrinterId printer_id) {
    PRINTER_LOG(EVENT) << entry.printer.make_and_model()
                       << " returned USB device ID: " << printer_id.raw_id();

    UpdateSearchDataFromDeviceId(printer_id, &entry);
    entry.ppd_search_data.printer_id = std::move(printer_id);

    // Add detected printer.
    if (session_controller_.IsScreenLocked()) {
      printers_locked_screen_[guid] = entry;
    } else {
      printers_ready_[guid] = entry;
      if (on_printers_found_callback_) {
        on_printers_found_callback_.Run(GetPrinters());
      }
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
    if (printers_ready_.erase(device_info->guid)) {
      if (on_printers_found_callback_) {
        on_printers_found_callback_.Run(GetPrinters());
      }
    } else {
      printers_locked_screen_.erase(device_info->guid);
    }
  }

  // ash::SessionObserver implementation.
  void OnLockStateChanged(bool locked) override {
    if (locked || printers_locked_screen_.empty()) {
      return;
    }
    printers_ready_.merge(printers_locked_screen_);
    printers_locked_screen_.clear();
    if (on_printers_found_callback_) {
      on_printers_found_callback_.Run(GetPrinters());
    }
  }

  SEQUENCE_CHECKER(sequence_);

  // Map from USB GUID to DetectedPrinter for all detected printers. Printers
  // detected when the screen is locked are saved in `printers_locked_screen_`.
  // They are later moved to `printers_ready_` when the screen is unlocked.
  // This is required because locking the screen activates usbguard that blocks
  // access to USB ports, so we have to defer installation of USB printers.
  std::map<std::string, DetectedPrinter> printers_ready_;
  std::map<std::string, DetectedPrinter> printers_locked_screen_;

  OnPrintersFoundCallback on_printers_found_callback_;

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};
  SessionControllerWrapper session_controller_;
  base::WeakPtrFactory<UsbPrinterDetectorImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<UsbPrinterDetector> UsbPrinterDetector::Create() {
  // Bind to the DeviceService for USB device manager.
  mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
  content::GetDeviceService().BindUsbDeviceManager(
      usb_manager.InitWithNewPipeAndPassReceiver());
  return std::make_unique<UsbPrinterDetectorImpl>(std::move(usb_manager));
}

std::unique_ptr<UsbPrinterDetector> UsbPrinterDetector::CreateForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager,
    ash::SessionController* session_controller) {
  return std::make_unique<UsbPrinterDetectorImpl>(std::move(usb_manager),
                                                  session_controller);
}

}  // namespace ash
