// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_webui_test_base.js']);
GEN_INCLUDE(['../testing/fake_objects.js']);

GEN('#include "content/public/test/browser_test.h"');

// Fake out the Chrome API namespace we depend on.
var chrome = {};
/** Fake chrome.brailleDisplayPrivate object. */
chrome.brailleDisplayPrivate = {};
/** Fake chrome.bluetooth object. */
chrome.bluetooth = {};
chrome.bluetooth.getDevices = (callback) => {};
chrome.bluetooth.onDeviceAdded = new FakeChromeEvent();
chrome.bluetooth.onDeviceChanged = new FakeChromeEvent();
chrome.bluetooth.onDeviceRemoved = new FakeChromeEvent();
chrome.bluetooth.startDiscovery = () => {};
chrome.bluetooth.stopDiscovery = () => {};
/** Fake chrome.bluetoothPrivate object. */
chrome.bluetoothPrivate = {};
chrome.bluetoothPrivate.onPairing = new FakeChromeEvent();
/** Fake chrome.accessibilityPrivate object. */
chrome.accessibilityPrivate = {};
chrome.metricsPrivate = {};
chrome.metricsPrivate.recordUserAction = function() {};

/**
 * Test fixture.
 */
ChromeVoxBluetoothBrailleDisplayUIWebUITest =
    class extends ChromeVoxWebUITestBase {
  /** @override */
  setUp() {
    Msgs = TestMsgs;
  }

  /** Label of the select. @type {string} */
  get selectLabel() {
    return 'Select a bluetooth braille display';
  }

  /**
   * Builds an expected stringified version of the widget, inserting static
   * expected content as needed.
   * @param {string} controls The expected controls block.
   * @return {string} The final expectation.
   */
  buildUIExpectation(controls) {
    return `
        <div>
          <h2>Bluetooth Braille Display</h2>
          <div class="option">
            <span id="bluetoothBrailleSelectLabel">${this.selectLabel}</span>
            ${controls}
          </div>
        </div>`;
  }
};


/** @override */
ChromeVoxBluetoothBrailleDisplayUIWebUITest.prototype.closureModuleDeps = [
  'BluetoothBrailleDisplayManager',
  'BluetoothBrailleDisplayUI',
  'TestMsgs',
];

ChromeVoxBluetoothBrailleDisplayUIWebUITest.prototype.isAsync = true;

SYNC_TEST_F(
    'ChromeVoxBluetoothBrailleDisplayUIWebUITest', 'NoDisplays', function() {
      const ui = new BluetoothBrailleDisplayUI();
      ui.attach(document.body);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel"></select>
                <button id="connectOrDisconnect" disabled="">Connect</button>
                <button id="forget" disabled="">Forget</button>`),
          document.body.children[0]);
    });

SYNC_TEST_F(
    'ChromeVoxBluetoothBrailleDisplayUIWebUITest',
    'ControlStateUpdatesNotConnectedOrPaired', function() {
      const ui = new BluetoothBrailleDisplayUI();
      ui.attach(document.body);

      let displays = [];

      // Fake out getDevice using |display| as the backing source which changes
      // below.
      chrome.bluetooth.getDevice = (address, callback) => {
        const display = displays.find((display) => display.address === address);
        assertNotNullNorUndefined(display);
        callback(display);
      };

      // One display; it automatically gets selected.
      // Not connected, not paired.
      displays = [{name: 'Focus 40 BT', address: 'abcd1234'}];
      ui.onDisplayListChanged(displays);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel">
                <option id="abcd1234"><span>Focus 40 BT</span></option>
              </select>
              <button id="connectOrDisconnect">Connect</button>
              <button id="forget" disabled="">Forget</button>`),
          document.body.children[0]);
      ui.detach();
    });

SYNC_TEST_F(
    'ChromeVoxBluetoothBrailleDisplayUIWebUITest',
    'ControlStateUpdatesPairedNotConnected', function() {
      const ui = new BluetoothBrailleDisplayUI();
      ui.attach(document.body);

      const display = [];

      // Fake out getDevice using |display| as the backing source which changes
      // below.
      chrome.bluetooth.getDevice = (address, callback) => {
        const display = displays.find((display) => display.address === address);
        assertNotNullNorUndefined(display);
        callback(display);
      };

      // One display; paired, but not connected.
      displays = [{name: 'Focus 40 BT', address: 'abcd1234', paired: true}];
      ui.onDisplayListChanged(displays);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel">
                <option id="abcd1234"><span>Focus 40 BT</span></option>
              </select>
              <button id="connectOrDisconnect">Connect</button>
              <button id="forget">Forget</button>`),
          document.body.children[0]);

      // Added one display; not paired, not connected.
      displays = [
        {name: 'Focus 40 BT', address: 'abcd1234', paired: true},
        {name: 'Focus 40 BT rev 2', address: '4321dcba'}
      ];
      ui.onDisplayListChanged(displays);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel">
                <option id="abcd1234"><span>Focus 40 BT</span></option>
                <option id="4321dcba"><span>Focus 40 BT rev 2</span></option>
              </select>
              <button id="connectOrDisconnect">Connect</button>
              <button id="forget">Forget</button>`),
          document.body.children[0]);

      // Our selected display is connecting.
      displays[0].connecting = true;
      ui.onDisplayListChanged(displays);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel" disabled="">
                <option id="abcd1234"><span>Focus 40 BT</span></option>
                <option id="4321dcba"><span>Focus 40 BT rev 2</span></option>
              </select>
              <button id="connectOrDisconnect" disabled="">Connecting</button>
              <button id="forget" disabled="">Forget</button>`),
          document.body.children[0]);

      // Our selected display connected.
      displays[0].connecting = false;
      displays[0].connected = true;
      ui.onDisplayListChanged(displays);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel">
                <option id="abcd1234"><span>Focus 40 BT</span></option>
                <option id="4321dcba"><span>Focus 40 BT rev 2</span></option>
              </select>
              <button id="connectOrDisconnect">Disconnect</button>
              <button id="forget">Forget</button>`),
          document.body.children[0]);

      // The user picks the second display.
      // The manager has to ask for the device details.
      const select = document.body.querySelector('select');
      select.selectedIndex = 1;
      const changeEvt = document.createEvent('HTMLEvents');
      changeEvt.initEvent('change');
      select.dispatchEvent(changeEvt);
      // The controls update based on the newly selected display.
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel">
                <option id="abcd1234"><span>Focus 40 BT</span></option>
                <option id="4321dcba"><span>Focus 40 BT rev 2</span></option>
              </select>
              <button id="connectOrDisconnect">Connect</button>
              <button id="forget" disabled="">Forget</button>`),
          document.body.children[0]);
    });

SYNC_TEST_F(
    'ChromeVoxBluetoothBrailleDisplayUIWebUITest', 'PincodeRequest',
    function() {
      const ui = new BluetoothBrailleDisplayUI();
      ui.attach(document.body);

      // Trigger pincode screen.
      ui.onPincodeRequested();
      assertEqualsDOM(
          `
          <div>
            <h2>Bluetooth Braille Display</h2>
            <form>
              <label id="pincodeLabel">Please enter a pin</label>
              <input id="pincode" type="text" aria-labelledby="pincodeLabel">
            </form>
            <div class="option" hidden="">
              <span id="bluetoothBrailleSelectLabel">${this.selectLabel}</span>
              <select aria-labelledby="bluetoothBrailleSelectLabel"></select>
              <button id="connectOrDisconnect" disabled="">Connect</button>
              <button id="forget" disabled="">Forget</button>
            </div>
          </div>`,
          document.body.children[0]);
      ui.detach();
    });

TEST_F(
    'ChromeVoxBluetoothBrailleDisplayUIWebUITest', 'ClickControls', function() {
      const ui = new BluetoothBrailleDisplayUI();
      ui.attach(document.body);

      let displays = [];

      // Fake out getDevice using |display| as the backing source which changes
      // below.
      chrome.bluetooth.getDevice = (address, callback) => {
        const display = displays.find((display) => display.address === address);
        assertNotNullNorUndefined(display);
        callback(display);
      };

      // One display; paired, but not connected.
      displays = [{name: 'VarioUltra', address: 'abcd1234', paired: true}];
      ui.onDisplayListChanged(displays);
      assertEqualsDOM(
          this.buildUIExpectation(`
              <select aria-labelledby="bluetoothBrailleSelectLabel">
                <option id="abcd1234"><span>VarioUltra</span></option>
              </select>
              <button id="connectOrDisconnect">Connect</button>
              <button id="forget">Forget</button>`),
          document.body.children[0]);

      // Click the connect button. Only connect should be called.
      chrome.bluetoothPrivate.connect = this.newCallback();
      chrome.bluetoothPrivate.disconnectAll = assertNotReached;
      document.getElementById('connectOrDisconnect').onclick();

      // Now, update the state to be connected.
      displays[0].connected = true;
      ui.onDisplayListChanged(displays);

      // Click the disconnect button.
      chrome.bluetoothPrivate.connect = assertNotReached;
      chrome.bluetoothPrivate.disconnectAll = this.newCallback();
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress =
          this.newCallback();
      document.getElementById('connectOrDisconnect').onclick();

      // Click the forget button.
      chrome.bluetoothPrivate.forgetDevice = this.newCallback();
      chrome.brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress =
          this.newCallback();
      document.getElementById('forget').onclick();
    });
