# Bluetooth Quick Settings UI

This directory contains Bluetooth system tray classes, used to display
information about the current state of Bluetooth adapter, list Bluetooth
devices that are currently paired, previously paired to and currently being
paired to. It allows a user to interact with Bluetooth devices, triggers
notifications (toast and system notifications) on the current status of a
Bluetooth device and opens a dialog to pair with a Bluetooth device.

Underneath the hood, the classes use the [CrosBluetoothConfig mojo API](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/Bluetooth_config/public/mojom/cros_Bluetooth_config.mojom;l=1;bpv=1;bpt=0;drc=321047b607bc69f5d6dce6e47319d0c198d0616e)
to fetch metadata about available Bluetooth devices.

## BluetoothFeaturePodController
This class governs the Bluetooth feature tile, offering options to control
Bluetooth status and access a detailed Bluetooth device list page.

## BluetoothDetailedViewController
This class serves as the core logic for managing the detailed Bluetooth
settings page within the quick settings panel. It translates user interactions
into Bluetooth state changes and maintains the distinction between previously
connected and currently connected devices. Additionally, it listens for
Bluetooth device property changes and instructs the
BluetoothDeviceListController to update the device list view accordingly.

## BluetoothDeviceListController
This class governs the device list within the detailed Bluetooth settings page,
enabling the addition, modification, and removal of devices. It manages the
subheader views for connected, unconnected, and previously connected devices,
and encompasses the factory method for generating instances of its
implementations.

## BluetoothDetailedView
This class serves as a central hub for managing the detailed Bluetooth settings
page within the quick settings panel. It houses the device list view,
establishes the delegate interface for handling user interactions, and provides
a factory method for creating instances of its implementations.

## BluetoothDeviceListItemView
This class encapsulates the logic of configuring the view shown for a single
device in the detailed Bluetooth page within the quick settings.

## BluetoothDeviceStatusUiHandler
This class monitors Bluetooth device connections and notifies the user through
pop-up messages when a device is paired, connected, or disconnected.

## BluetoothNotificationController
This class acts as an intermediary between the BluetoothAdapter and the user,
handling incoming pairing requests, adapter state changes, and bonded device
updates. It presents pairing requests as interactive notifications that allow
users to accept or decline the pairing. These interactions are managed by the
BluetoothPairingNotificationDelegate.
