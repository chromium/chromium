# ChromeOS Bluetooth Pairing UI

This directory contains Bluetooth pairing UI polymer elements used to display
information about available Bluetooth devices that can be paired, and UI that
the user interacts with to pair with a Bluetooth device of their choosing.
The dialog is either shown within Settings UI, a standalone dialog in sign-in
screen and OOBE.

Underneath the hood, the elements use the [CrosBluetoothConfig mojo API](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/Bluetooth_config/public/mojom/cros_Bluetooth_config.mojom;l=1;bpv=1;bpt=0;drc=321047b607bc69f5d6dce6e47319d0c198d0616e)
to fetch metadata about available Bluetooth devices to pair with, and to
actually pair with Bluetooth devices.

## BluetoothBasePage
Base template with elements common to all Bluetooth UI sub-pages.

## BluetoothBatteryIconPercentage
View displaying a dynamically colored/sized battery icon and corresponding
battery percentage string for a given device and battery type.

## BluetoothDeviceBatteryInfo
View displaying Bluetooth device battery info. Decides whether to show multiple
battery icon percentages (if the Bluetooth device has multiple associated
batteries, like wireless earbuds for example) or a single battery icon
percentage (like a single Bluetooth speaker for example).

## BluetoothIcon
UI element used to display Bluetooth device icon. Decides whether to show
system Bluetooth icons depending on the type of device, or the default
device image if there is an available image url associated to the device.

## BluetoothMetricsUtils
Used by other components in this directory to record Bluetooth metrics.

## BlueoothPairingConfirmCodePage
Bluetooth page that displays UI elements for when authentication via
confirm passkey is required during Bluetooth device pairing.

## BluetoothPairingDeviceItem
Container used to display information about a single Bluetooth device.

## BluetoothPairingDeviceSelectionPage
Bluetooth page that displays a list of discovered Bluetooth devices
and initiate pairing to a device.

## BluetoothPairingEnterCodePage
Bluetooth page that displays UI elements for when authentication via
display passkey or PIN is required during Bluetooth device pairing.

## BluetoothPairingRequestCodePage
Bluetooth page that displays UI elements for when authentication via PIN
or PASSKEY is required during Bluetooth device pairing.

## BluetoothPairingUi
Root UI element for Bluetooth pairing dialog. Contains all the Bluetooth
pairing pages and decides which one to display.

## BluetoothSpinnerPage
Bluetooth page displayed when a pairing is in progress. Displays a
pinwheel.

## BluetoothTypes
Contains enums that are used to describe the type and state of the
Bluetooth device.

## BluetoothUtils
Contains utility functions to easily fetch metadata about a
Bluetooth device.

## CrosBluetoothConfig
Wrapper for CrosBluetoothConfig that provides the ability to inject
a fake CrosBluetoothConfig implementation for tests.