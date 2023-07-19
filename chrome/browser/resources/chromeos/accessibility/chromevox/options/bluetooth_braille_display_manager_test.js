// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);
GEN_INCLUDE(['../testing/fake_objects.js']);

/**
 * A fake BluetoothBraileDisplayManagerListener.
 */
class FakeBluetoothBrailleDisplayManagerListener {
  constructor() {
    this.displays = [];
    this.wasPincodeRequested = false;
  }

  onDisplayListChanged(displays) {
    this.displays = displays;
  }

  onPincodeRequested(displays) {
    this.wasPincodeRequested = true;
  }
}


/**
 * Test fixture.
 */
ChromeVoxBluetoothBrailleDisplayManagerWebUITest =
    class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    await Promise.all([
      // Alphabetical based on file path.
      importModule(
          'BluetoothBrailleDisplayManager',
          '/chromevox/options/bluetooth_braille_display_manager.js'),
      importModule('LocalStorage', '/common/local_storage.js'),
      importModule('SettingsManager', '/chromevox/common/settings_manager.js'),
    ]);
  }
};

ChromeVoxBluetoothBrailleDisplayManagerWebUITest.prototype.isAsync = true;
TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest', 'Connect', function() {
      let connectCalled = false;
      chrome.bluetoothPrivate.connect = (result, callback) => {
        connectCalled = true;
        callback();
      };
      chrome.bluetoothPrivate.disconnectAll = assertNotReached;
      chrome.bluetoothPrivate.pair = this.newCallback();
      const manager = new BluetoothBrailleDisplayManager();
      manager.connect({address: 'abcd', connected: false, paired: false});
      assertTrue(connectCalled);
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest', 'ConnectAlreadyPaired',
    function() {
      chrome.bluetoothPrivate.connect = this.newCallback();
      chrome.bluetoothPrivate.disconnectAll = assertNotReached;
      chrome.bluetoothPrivate.pair = assertNotReached;
      const manager = new BluetoothBrailleDisplayManager();
      manager.connect({address: 'abcd', connected: false, paired: true});
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest',
    'ConnectAlreadyConnectedNotPaired', function() {
      chrome.bluetoothPrivate.connect = assertNotReached;
      chrome.bluetoothPrivate.disconnectAll = assertNotReached;
      chrome.bluetoothPrivate.pair = this.newCallback();
      const manager = new BluetoothBrailleDisplayManager();
      manager.connect({address: 'abcd', connected: true, paired: false});
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest',
    'DisconnectPreviousPreferredOnConnectNewPreferred', function() {
      chrome.bluetoothPrivate.connect = this.newCallback(address => {
        assertEquals('abcd', address);
      });
      chrome.bluetoothPrivate.disconnectAll =
          this.newCallback((address, callback) => {
            assertEquals('1234', address);
            callback();
          });
      SettingsManager.set('preferredBrailleDisplayAddress', '1234');
      const manager = new BluetoothBrailleDisplayManager();
      manager.connect({address: 'abcd', connected: false, paired: false});
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest', 'ReconnectPreferred',
    function() {
      chrome.bluetoothPrivate.connect = this.newCallback();
      chrome.bluetoothPrivate.disconnectAll = assertNotReached;
      SettingsManager.set('preferredBrailleDisplayAddress', 'abcd');
      const manager = new BluetoothBrailleDisplayManager();
      manager.connect({address: 'abcd', connected: false, paired: false});
    });

AX_TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest', 'Listener', function() {
      const manager = new BluetoothBrailleDisplayManager();
      const listener = new FakeBluetoothBrailleDisplayManagerListener();
      manager.addListener(listener);
      let devices = [];
      chrome.bluetooth.getDevices = callback => callback(devices);

      // No devices have been added, removed, or changed.
      manager.handleDevicesChanged();
      assertEquals(0, listener.displays.length);

      // A recognized braille display was added.
      devices = [{name: 'Focus 40 BT', address: '1234'}];
      manager.handleDevicesChanged();
      assertEquals(1, listener.displays.length);
      assertEquals('Focus 40 BT', listener.displays[0].name);

      // An unrecognized device was added.
      devices = [
        {name: 'Focus 40 BT', address: '1234'},
        {name: 'headphones', address: '4321'},
      ];
      manager.handleDevicesChanged();
      assertEquals(1, listener.displays.length);
      assertEquals('Focus 40 BT', listener.displays[0].name);

      // A named variant of Focus 40 BT was added.
      devices = [
        {name: 'Focus 40 BT', address: '1234'},
        {name: 'Focus 40 BT rev 123', address: '4321'},
      ];
      manager.handleDevicesChanged();
      assertEquals(2, listener.displays.length);
      assertEquals('Focus 40 BT', listener.displays[0].name);
      assertEquals('Focus 40 BT rev 123', listener.displays[1].name);
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest',
    'ConnectPreferredTriggersBrlttyUpdate', function() {
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress =
          this.newCallback(address => {
            assertEquals('abcd', address);
          });

      SettingsManager.set('preferredBrailleDisplayAddress', 'abcd');
      const manager = new BluetoothBrailleDisplayManager();
      let devices = [];
      chrome.bluetooth.getDevices = callback => callback(devices);

      // No devices.
      manager.handleDevicesChanged();

      // A different device.
      devices = [{name: 'Headhpones', address: '1234', connected: true}];
      manager.handleDevicesChanged();

      // A focus, but different address.
      devices = [{name: 'Focus 40 BT', address: '1234', connected: true}];
      manager.handleDevicesChanged();

      // Finally, a device that is both connected and is our preferred device.
      devices = [{name: 'Focus 40 BT', address: 'abcd', connected: true}];
      manager.handleDevicesChanged();
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest',
    'ForgetPreferredTriggersBrlttyUpdate', function() {
      chrome.bluetoothPrivate.forgetDevice = this.newCallback();
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress =
          this.newCallback(address => {
            assertEquals('', address);
          });

      const manager = new BluetoothBrailleDisplayManager();

      // Forget the preferred device. Note there is no requirement that this
      // device be preferred.
      manager.forget('abcd');
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayManagerWebUITest',
    'DisconnectPreferredTriggersBrlttyUpdate', function() {
      chrome.bluetoothPrivate.disconnectAll = this.newCallback();
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress =
          this.newCallback(address => {
            assertEquals('', address);
          });

      const manager = new BluetoothBrailleDisplayManager();

      // Disconnect the preferred device. Note there is no requirement that this
      // device be preferred.
      manager.disconnect('abcd');
    });
