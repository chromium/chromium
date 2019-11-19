// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './icons.js';
import './shared_styles.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/**
 * A bluetooth device.
 * @constructor
 */
var BluetoothDevice = function() {
  // The device's address (MAC format, must be unique).
  this.address = '';

  // The label which shows up in the devices list for this device.
  this.alias = '';

  // The text label of the selected device class.
  this.class = 'Computer';

  // The uint32 value of the selected device class.
  this.classValue = 0x104;

  // Whether or not the device shows up in the system tray's observed list of
  // bluetooth devices.
  this.discoverable = false;

  // Whether Chrome OS pairs with this device, or this device tries to pair
  // with Chrome OS.
  this.incoming = false;

  // A trusted device is one which is plugged directly Chrome OS and therefore
  // is paired by default, but not connected.
  this.isTrusted = false;

  // The device's name.This is not the label which shows up in the devices list
  // here or in the system tray--use |.alias| to edit that label.
  this.name = '';

  // The designated path for the device. Must be unique.
  this.path = '';

  // Whether or not the device is paired with Chrome OS.
  this.paired = false;

  // The label of the selected pairing method option.
  this.pairingMethod = 'None';

  // The text containing a PIN key or passkey for pairing.
  this.pairingAuthToken = '';

  // The label of the selected pairing action option.
  this.pairingAction = '';
};

Polymer({
  is: 'bluetooth-settings',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * A set of bluetooth devices.
     * @type !Array<!BluetoothDevice>
     */
    devices: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * A set of predefined bluetooth devices.
     * @type !Array<!BluetoothDevice>
     */
    predefinedDevices: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * A bluetooth device object which is currently being edited.
     * @type {?BluetoothDevice}
     */
    currentEditableObject: {
      type: Object,
      value: null,
    },

    /**
     * The index of the bluetooth device object which is currently being edited.
     * This is initially set to -1 (i.e. no device selected) because not custom
     * devices exist when the page loads.
     */
    currentEditIndex: {
      type: Number,
      value: function() {
        return -1;
      }
    },

    /**
     * A set of options for the possible bluetooth device classes/types.
     * Object |value| attribute comes from values in the WebUI, set in
     * setDeviceClassOptions.
     * @type !Array<!{text: string, value: number}>
     */
    deviceClassOptions: {
      type: Array,
      value: function() {
        return [
          {text: 'Unknown', value: 0}, {text: 'Mouse', value: 0x2580},
          {text: 'Keyboard', value: 0x2540}, {text: 'Audio', value: 0x240408},
          {text: 'Phone', value: 0x7a020c}, {text: 'Computer', value: 0x104}
        ];
      }
    },

    /**
     * A set of strings representing the method to be used for
     * authenticating a device during a pair request.
     * @type !Array<string>
     */
    deviceAuthenticationMethods: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * A set of strings representing the actions which can be done when
     * a secure device is paired/requests a pair.
     * @type !Array<string>
     */
    deviceAuthenticationActions: {
      type: Array,
      value: function() {
        return [];
      }
    },
  },

  /**
   * Contains keys for all the device paths which have been discovered. Used
   * to look up whether or not a device is listed already.
   * @type {Object}
   */
  devicePaths: {},

  ready: function() {
    this.addWebUIListener(
        'bluetooth-device-added', this.addBluetoothDevice_.bind(this));
    this.addWebUIListener(
        'device-paired-from-tray', this.devicePairedFromTray_.bind(this));
    this.addWebUIListener(
        'device-removed-from-main-adapter',
        this.deviceRemovedFromMainAdapter_.bind(this));
    this.addWebUIListener('pair-failed', this.pairFailed_.bind(this));
    this.addWebUIListener(
        'bluetooth-info-updated', this.updateBluetoothInfo_.bind(this));
    chrome.send('requestBluetoothInfo');
  },

  observers: ['currentEditableObjectChanged(currentEditableObject.*)'],

  /**
   * Called when a property of the currently editable object is edited.
   * Sets the corresponding property for the object in |this.devices|.
   * @param {Object} obj An object containing event information (ex. which
   *     property of |this.currentEditableObject| was changed, what its value
   *     is, etc.)
   */
  currentEditableObjectChanged: function(obj) {
    if (this.currentEditIndex >= 0) {
      var prop = obj.path.split('.')[1];
      this.set(
          'devices.' + this.currentEditIndex.toString() + '.' + prop,
          obj.value);
    }
  },

  handleAddressInput: function() {
    this.autoFormatAddress();
    this.validateAddress();
  },

  autoFormatAddress: function() {
    var input = this.$.deviceAddressInput;
    var regex = /([a-f0-9]{2})([a-f0-9]{2})/i;
    // Remove things that aren't hex characters from the string.
    var val = input.value.replace(/[^a-f0-9]/ig, '');

    // Insert a ':' in the middle of every four hex characters.
    while (regex.test(val))
      val = val.replace(regex, '$1:$2');

    input.value = val;
  },

  /**
   * Called on-input from an input element and on edit dialog open.
   * Validates whether or not the
   * input's content matches a regular expression. If the input's value
   * satisfies the regex, then make sure that the address is not already
   * in use.
   */
  validateAddress: function() {
    var input = this.$.deviceAddressInput;
    var val = input.value;
    var exists = false;
    var addressRegex = RegExp('^([\\da-fA-F]{2}:){5}[\\da-fA-F]{2}$');
    if (addressRegex.test(val)) {
      for (var i = 0; i < this.predefinedDevices.length; ++i) {
        if (this.predefinedDevices[i].address == val) {
          exists = true;
          break;
        }
      }

      if (!exists) {
        for (var i = 0; i < this.devices.length; ++i) {
          if (this.devices[i].address == val && i != this.currentEditIndex) {
            exists = true;
            break;
          }
        }
      }

      if (exists) {
        input.invalid = true;
        input.errorMessage = 'This address is already being used.';
      } else {
        input.invalid = false;
      }
    } else {
      input.invalid = true;
      input.errorMessage = 'Invalid address.';
    }
  },

  /**
   * Makes sure that a path is not already used.
   */
  validatePath: function() {
    var input = this.$.devicePathInput;
    var val = input.value;
    var exists = false;

    for (var i = 0; i < this.predefinedDevices.length; ++i) {
      if (this.predefinedDevices[i].path == val) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      for (var i = 0; i < this.devices.length; ++i) {
        if (this.devices[i].path == val && i != this.currentEditIndex) {
          exists = true;
          break;
        }
      }
    }

    if (exists) {
      input.invalid = true;
      input.errorMessage = 'This path is already being used.';
    } else {
      input.invalid = false;
    }
  },

  /**
   * Checks whether or not the PIN/passkey input field should be shown.
   * It should only be shown when the pair method is not 'None' or empty.
   * @param {string} pairMethod The label of the selected pair method option
   *     for a particular device.
   * @return {boolean} Whether the PIN/passkey input field should be shown.
   */
  showAuthToken: function(pairMethod) {
    return !!pairMethod && pairMethod != 'None';
  },

  /**
   * Called by the WebUI which provides a list of devices which are connected
   * to the main adapter.
   * @param {{
   *   predefined_devices: !Array<!BluetoothDevice>,
   *   devices: !Array<!BluetoothDevice>,
   *   pairing_method_options: !Array<string>,
   *   pairing_action_options: !Array<string>,
   * }} info
   * @private
   */
  updateBluetoothInfo_: function(info) {
    this.predefinedDevices =
        this.loadDevicesFromList(info.predefined_devices, true);
    this.devices = this.loadDevicesFromList(info.devices, false);
    this.deviceAuthenticationMethods = info.pairing_method_options;
    this.deviceAuthenticationActions = info.pairing_action_options;
  },

  /**
   * Builds complete BluetoothDevice objects for each element in |devices_list|.
   * @param {!Array<!BluetoothDevice>} devices A list of incomplete
   *     BluetoothDevice provided by the C++ WebUI.
   * @param {boolean} predefined Whether or not the device is a predefined one.
   */
  loadDevicesFromList: function(devices, predefined) {
    /** @type {!Array<!BluetoothDevice>} */ var deviceList = [];

    for (var i = 0; i < devices.length; ++i) {
      if (this.devicePaths[devices[i].path] != undefined)
        continue;

      // Get the label for the device class which should be selected.
      devices[i].class = this.getTextForDeviceClass(devices[i].classValue);
      devices[i].pairingAuthToken = devices[i].pairingAuthToken.toString();
      deviceList.push(devices[i]);
      this.devicePaths[devices[i].path] = {
        predefined: predefined,
        index: deviceList.length - 1
      };
    }

    return deviceList;
  },

  /**
   * Called when a device is paired from the Tray. Checks the paired box for
   * the device with path |path|.
   * @private
   */
  devicePairedFromTray_: function(path) {
    var obj = this.devicePaths[path];

    if (obj == undefined)
      return;

    var index = obj.index;
    var devicePath = (obj.predefined ? 'predefinedDevices.' : 'devices.');
    devicePath += obj.index.toString();
    this.set(devicePath + '.paired', true);
  },

  /**
   * On-change handler for a checkbox in the device list. Pairs/unpairs the
   * device associated with the box checked/unchecked.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  pairDevice: function(event) {
    var index = event.model.index;
    var predefined =
        /** @type {boolean} */ (event.target.dataset.predefined == 'true');
    var device =
        predefined ? this.predefinedDevices[index] : this.devices[index];

    if (event.target.checked) {
      var devicePath = (predefined ? 'predefinedDevices.' : 'devices.');
      devicePath += index.toString();
      this.set(devicePath + '.discoverable', true);

      // Send device info to the WebUI.
      chrome.send('requestBluetoothPair', [device]);
      this.devicePaths[device.path] = {predefined: predefined, index: index};

      var devicePath = (predefined ? 'predefinedDevices.' : 'devices.');
      devicePath += index.toString();
      this.set(devicePath + '.paired', false);
    } else {
      chrome.send('removeBluetoothDevice', [device.path]);

      var devicePath = (predefined ? 'predefinedDevices.' : 'devices.');
      devicePath += index.toString();
      this.set(devicePath + '.discoverable', false);
    }
  },

  /**
   * Called from Chrome OS back-end when a pair request fails.
   * @param {string} path The path of the device which failed to pair.
   * @private
   */
  pairFailed_: function(path) {
    var obj = this.devicePaths[path];

    if (obj == undefined)
      return;

    var devicePath = (obj.predefined ? 'predefinedDevices.' : 'devices.');
    devicePath += obj.index.toString();
    this.set(devicePath + '.paired', false);
  },

  /**
   * On-change event handler for a checkbox in the device list.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  discoverDevice: function(event) {
    var index = event.model.index;
    var predefined =
        /** @type {boolean} */ (event.target.dataset.predefined == 'true');
    var device =
        predefined ? this.predefinedDevices[index] : this.devices[index];

    if (event.target.checked) {
      device.classValue = this.getValueForDeviceClass(device.class);

      // Send device info to WebUI.
      chrome.send('requestBluetoothDiscover', [device]);

      this.devicePaths[device.path] = {predefined: predefined, index: index};
    } else {
      chrome.send('removeBluetoothDevice', [device.path]);

      var devicePath = (predefined ? 'predefinedDevices.' : 'devices.');
      devicePath += index.toString();
      this.set(devicePath + '.paired', false);
    }
  },

  // Adds a new device with default settings to the list of devices.
  appendNewDevice: function() {
    var newDevice = new BluetoothDevice();
    newDevice.alias = 'New Device';
    this.push('devices', newDevice);
  },

  /**
   * This is called when a new device is discovered by the main adapter.
   * The device is only added to the view's list if it is not already in
   * the list (i.e. its path has not yet been recorded in |devicePaths|).
   * @param {BluetoothDevice} device A bluetooth device.
   * @private
   */
  addBluetoothDevice_: function(device) {
    if (this.devicePaths[device.path] != undefined) {
      var obj = this.devicePaths[device.path];
      var devicePath = (obj.predefined ? 'predefinedDevices.' : 'devices.');
      devicePath += obj.index.toString();
      this.set(devicePath + '.discoverable', true);
      return;
    }

    device.class = this.getTextForDeviceClass(device.classValue);
    device.discoverable = true;
    this.push('devices', device);
    this.devicePaths[device.path] = {
      predefined: false,
      index: this.devices.length - 1
    };
  },

  /**
   * Called on "copy" button from the device list clicked. Creates a copy of
   * the selected device and adds it to the "custom" devices list.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  copyDevice: function(event) {
    var predefined = (event.target.dataset.predefined == 'true');
    var index = event.model.index;
    var copyDevice =
        predefined ? this.predefinedDevices[index] : this.devices[index];
    // Create a deep copy of the selected device.
    var newDevice = new BluetoothDevice();
    Object.assign(newDevice, copyDevice);
    newDevice.path = '';
    newDevice.address = '';
    newDevice.name += ' (Copy)';
    newDevice.alias += ' (Copy)';
    newDevice.discoverable = false;
    newDevice.paired = false;
    this.push('devices', newDevice);
  },

  /**
   * Shows a dialog to edit the selected device's properties.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  showEditDialog: function(event) {
    var index = event.model.index;
    this.currentEditIndex = index;
    this.currentEditableObject = this.devices[index];
    this.$.editDialog.showModal();
    this.validateAddress();
    this.validatePath();
  },

  /** @private */
  onCloseClick_: function() {
    this.$.editDialog.close();
  },

  /**
   * A click handler for the delete button on bluetooth devices.
   * @param {Event} event Contains event data. |event.model.index| is the index
   *     of the item which the target is contained in.
   */
  deleteDevice: function(event) {
    var index = event.model.index;
    var device = this.devices[index];

    chrome.send('removeBluetoothDevice', [device.path]);

    this.devicePaths[device.path] = undefined;
    this.splice('devices', index, 1);
  },

  /**
   * This function is called when a device is removed from the main bluetooth
   * adapter's device list. It sets that device's |.discoverable| and |.paired|
   * attributes to false.
   * @param {string} path A bluetooth device's path.
   * @private
   */
  deviceRemovedFromMainAdapter_: function(path) {
    if (this.devicePaths[path] == undefined)
      return;

    var obj = this.devicePaths[path];
    var devicePath = (obj.predefined ? 'predefinedDevices.' : 'devices.');
    devicePath += obj.index.toString();
    this.set(devicePath + '.discoverable', false);
    this.set(devicePath + '.paired', false);
  },

  /**
   * Returns the text for the label that corresponds to |classValue|.
   * @param {number} classValue A number representing the bluetooth class
   *     of a device.
   * @return {string} The label which represents |classValue|.
   */
  getTextForDeviceClass: function(classValue) {
    for (var i = 0; i < this.deviceClassOptions.length; ++i) {
      if (this.deviceClassOptions[i].value == classValue)
        return this.deviceClassOptions[i].text;
    }
    return '';
  },

  /**
   * Returns the integer value which corresponds with the label |classText|.
   * @param {string} classText The label for a device class option.
   * @return {number} The value which |classText| represents.
   */
  getValueForDeviceClass: function(classText) {
    for (var i = 0; i < this.deviceClassOptions.length; ++i) {
      if (this.deviceClassOptions[i].text == classText)
        return this.deviceClassOptions[i].value;
    }
    return 0;
  },
});
