// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
#define CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace chromeos {

const uint8_t kInvalidUsbPortNumber = 0xff;

// Reasons the notification may be closed. These are used in histograms so do
// not remove/reorder entries. Only add at the end just before kMaxValue. Also
// remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum class CrosUsbNotificationClosed {
  // The notification was dismissed but not by the user (either automatically
  // or because the device was unplugged).
  kUnknown,
  // The user closed the notification via the close box.
  kByUser,
  // The user clicked on the Connect to Linux button of the notification.
  kConnectToLinux,
  // Maximum value for the enum.
  kMaxValue = kConnectToLinux
};

// Represents a USB device tracked by a CrosUsbDetector instance. The device may
// or may not be sharable with a particular type of VM (e.g. Crostini).
struct CrosUsbDeviceInfo {
  CrosUsbDeviceInfo();
  CrosUsbDeviceInfo(const CrosUsbDeviceInfo&);
  ~CrosUsbDeviceInfo();

  std::string guid;
  base::string16 label;
  // Whether the device can be shared with guest OSes.
  bool sharable_with_crostini = false;
  // Name of VM shared with. Unset if not shared. Note that the device may be
  // shared but not attached (yet) in which case |guest_port| below would be
  // unset.
  base::Optional<std::string> shared_vm_name;
  base::Optional<uint8_t> guest_port;
  // Interfaces shareable with guest OSes
  uint32_t allowed_interfaces_mask = 0;
  // TODO(nverne): Add current state and errors etc.
};

class CrosUsbDeviceObserver : public base::CheckedObserver {
 public:
  // Called when the available USB devices change.
  virtual void OnUsbDevicesChanged() = 0;
};

// Detects USB Devices for Chrome OS and manages UI for controlling their use
// with CrOS, Web or GuestOSs.
class CrosUsbDetector : public device::mojom::UsbDeviceManagerClient,
                        public chromeos::ConciergeClient::VmObserver,
                        public chromeos::VmPluginDispatcherClient::Observer {
 public:
  // Used to namespace USB notifications to avoid clashes with WebUsbDetector.
  static std::string MakeNotificationId(const std::string& guid);

  // Can return nullptr.
  static CrosUsbDetector* Get();

  CrosUsbDetector();
  ~CrosUsbDetector() override;

  void SetDeviceManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager);

  // Connect to the device manager to be notified of connection/removal.
  // Used during browser startup, after connection errors and to setup a fake
  // device manager during testing.
  void ConnectToDeviceManager();

  // Called when a VM starts, to attach USB devices marked as shared to the VM.
  void ConnectSharedDevicesOnVmStartup(const std::string& vm_name);

  // device::mojom::UsbDeviceManagerClient
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device) override;

  // Attaches the device identified by |guid| into the VM identified by
  // |vm_name|.
  void AttachUsbDeviceToVm(const std::string& vm_name,
                           const std::string& guid,
                           base::OnceCallback<void(bool success)> callback);

  // Detaches the device identified by |guid| from the VM identified by
  // |vm_name|.
  void DetachUsbDeviceFromVm(const std::string& vm_name,
                             const std::string& guid,
                             base::OnceCallback<void(bool success)> callback);

  // Returns true if device was successfully shared with |vm_name|.
  bool IsDeviceAlreadySharedWithVm(const std::string& vm_name,
                                   const std::string& guid);

  void AddUsbDeviceObserver(CrosUsbDeviceObserver* observer);
  void RemoveUsbDeviceObserver(CrosUsbDeviceObserver* observer);
  void SignalUsbDeviceObservers();

  // Returns all the USB devices tracked by this instance. This may not contain
  // all physically connected devices and may also contain devices that are
  // sharable with e.g. ARCVM but not with Crostini.
  const std::vector<CrosUsbDeviceInfo>& GetConnectedDevices() const;

  // Returns all the USB devices that are sharable with Crostini. This may not
  // include all connected devices.
  std::vector<CrosUsbDeviceInfo> GetDevicesSharableWithCrostini() const;

 private:
  // chromeos::ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // chromeos::VmPluginDispatcherClient::Observer:
  void OnVmToolsStateChanged(
      const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal)
      override;
  void OnVmStateChanged(
      const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) override;

  // Called after USB device access has been checked.
  void OnDeviceChecked(device::mojom::UsbDeviceInfoPtr device,
                       bool hide_notification,
                       bool allowed);

  // Allows the notification to be hidden (OnDeviceAdded without the flag calls
  // this).
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device,
                     bool hide_notification);
  void OnDeviceManagerConnectionError();

  // Callback listing devices attached to the machine.
  void OnListAttachedDevices(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);

  // Callback for AttachUsbDeviceToVm after opening a file handler.
  void OnAttachUsbDeviceOpened(const std::string& vm_name,
                               device::mojom::UsbDeviceInfoPtr device,
                               base::OnceCallback<void(bool success)> callback,
                               base::File file);

  void DoVmAttach(const std::string& vm_name,
                  device::mojom::UsbDeviceInfoPtr device_info,
                  base::ScopedFD fd,
                  base::OnceCallback<void(bool success)> callback);

  // Callbacks for when the USB device state has been updated.
  void OnUsbDeviceAttachFinished(
      const std::string& vm_name,
      const std::string& guid,
      base::OnceCallback<void(bool success)> callback,
      base::Optional<vm_tools::concierge::AttachUsbDeviceResponse> response);

  void OnUsbDeviceDetachFinished(
      const std::string& vm_name,
      const std::string& guid,
      base::OnceCallback<void(bool success)> callback,
      base::Optional<vm_tools::concierge::DetachUsbDeviceResponse> response);

  // Devices will be auto-detached if they are attached to another VM.
  void AttachAfterDetach(const std::string& vm_name,
                         const std::string& guid,
                         base::OnceCallback<void(bool success)> callback,
                         bool success);

  // Returns true when a device should show a notification when attached.
  bool ShouldShowNotification(const device::mojom::UsbDeviceInfo& device_info,
                              uint32_t allowed_interfaces_mask);

  void RelinquishDeviceClaim(const std::string& guid);

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};

  std::vector<device::mojom::UsbDeviceFilterPtr> guest_os_classes_blocked_;
  std::vector<device::mojom::UsbDeviceFilterPtr>
      guest_os_classes_without_notif_;
  device::mojom::UsbDeviceFilterPtr adb_device_filter_;
  device::mojom::UsbDeviceFilterPtr fastboot_device_filter_;

  // A mapping from GUID -> UsbDeviceInfo for each attached USB device
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> available_device_info_;

  // Populated when we open the device path on the host. Acts as a claim on the
  // device even if the intended VM has not started yet. Removed when the device
  // is shared successfully with the VM. When an file is closed (here or by the
  // VM,  PermissionBroker will reattach the previous host drivers (if any).
  struct DeviceClaim {
    base::File device_file;
    base::File lifeline_file;
  };
  std::map<std::string, DeviceClaim> devices_claimed_;

  std::vector<CrosUsbDeviceInfo> usb_devices_;

  base::ObserverList<CrosUsbDeviceObserver> usb_device_observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrosUsbDetector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosUsbDetector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USB_CROS_USB_DETECTOR_H_
