// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A widget that exposes UI for interacting with a list of braille
 * displays.
 */

goog.provide('BluetoothBrailleDisplayUI');

goog.require('BluetoothBrailleDisplayListener');
goog.require('BluetoothBrailleDisplayManager');
goog.require('Msgs');

/**
 * A widget used for interacting with bluetooth braille displays.
 * @constructor
 * @implements {BluetoothBrailleDisplayListener}
 */
BluetoothBrailleDisplayUI = function() {
  /** @private {!BluetoothBrailleDisplayManager} */
  this.manager_ = new BluetoothBrailleDisplayManager();

  this.manager_.addListener(this);

  /** @private {Element} */
  this.root_;

  /** @private {Element} */
  this.displaySelect_;

  /** @private {Element} */
  this.controls_;

  /** @private {string|undefined} */
  this.selectedAndConnectedDisplayAddress_;
};

BluetoothBrailleDisplayUI.prototype = {
  /**
   * Attaches this widget to |element|.
   * @param {!Element} element
   */
  attach: function(element) {
    this.manager_.start();
    var container = document.createElement('div');
    element.appendChild(container);
    this.root_ = container;

    var title = document.createElement('h2');
    title.textContent = Msgs.getMsg('options_bluetooth_braille_display_title');
    container.appendChild(title);

    var controls = document.createElement('div');
    container.appendChild(controls);
    this.controls_ = controls;
    controls.className = 'option';

    var selectLabel = document.createElement('span');
    controls.appendChild(selectLabel);
    selectLabel.id = 'bluetoothBrailleSelectLabel';
    selectLabel.textContent =
        Msgs.getMsg('options_bluetooth_braille_display_select_label');

    var displaySelect = document.createElement('select');
    this.displaySelect_ = displaySelect;
    controls.appendChild(displaySelect);
    displaySelect.setAttribute(
        'aria-labelledby', 'bluetoothBrailleSelectLabel');
    displaySelect.addEventListener('change', (evt) => {
      this.updateControls_();
    });

    var connectOrDisconnect = document.createElement('button');
    controls.appendChild(connectOrDisconnect);
    connectOrDisconnect.id = 'connectOrDisconnect';
    connectOrDisconnect.disabled = true;
    connectOrDisconnect.textContent =
        Msgs.getMsg('options_bluetooth_braille_display_connect');

    var forget = document.createElement('button');
    controls.appendChild(forget);
    forget.id = 'forget';
    forget.textContent =
        Msgs.getMsg('options_bluetooth_braille_display_forget');
    forget.disabled = true;
  },

  /**
   * Detaches the rendered widget.
   */
  detach: function() {
    this.manager_.stop();

    if (this.root_) {
      this.root_.remove();
      this.root_ = null;
    }
  },

  /** @override */
  onDisplayListChanged: function(displays) {
    if (!this.displaySelect_) {
      throw 'Expected attach to have been called.';
    }

    // Remove any displays that were removed.
    for (var i = 0; i < this.displaySelect_.children.length; i++) {
      var domDisplay = this.displaySelect_.children[i];
      if (!displays.find((display) => domDisplay.id == display.address)) {
        domDisplay.remove();
      }
    }

    displays.forEach((display) => {
      // Check if the element already exists.
      var displayContainer =
          this.displaySelect_.querySelector('#' + CSS.escape(display.address));

      // If the display already exists, no further processing is needed.
      if (displayContainer) {
        return;
      }

      displayContainer = document.createElement('option');
      this.displaySelect_.appendChild(displayContainer);
      displayContainer.id = display.address;
      var name = document.createElement('span');
      displayContainer.appendChild(name);
      name.textContent = display.name;
    });
    this.updateControls_();
  },

  /** @override */
  onPincodeRequested: function(display) {
    this.controls_.hidden = true;
    var form = document.createElement('form');
    this.controls_.parentElement.insertBefore(form, this.controls_);

    // Create the text field and its label.
    var label = document.createElement('label');
    form.appendChild(label);
    label.id = 'pincodeLabel';
    label.textContent =
        Msgs.getMsg('options_bluetooth_braille_display_pincode_label');
    label.for = 'pincode';
    var pincodeField = document.createElement('input');
    pincodeField.id = 'pincode';
    pincodeField.type = 'text';
    pincodeField.setAttribute('aria-labelledby', 'pincodeLabel');
    form.appendChild(pincodeField);

    var timeoutId;
    form.addEventListener('submit', (evt) => {
      if (timeoutId) {
        clearTimeout(timeoutId);
      }

      if (pincodeField.value) {
        this.manager_.finishPairing(display, pincodeField.value);
      }

      this.controls_.hidden = false;
      form.remove();
      form = null;
      evt.preventDefault();
      evt.stopPropagation();
      this.displaySelect_.focus();
    });

    // Also, schedule a 60 second timeout for pincode entry.
    timeoutId = setTimeout(() => {
      form.remove();
      this.controls_.hidden = false;
      this.displaySelect_.focus();
    }, 60000);

    document.body.blur();
    pincodeField.focus();
  },

  /**
   * @private
   */
  updateControls_: function() {
    // Only update controls if there is a selected display.
    var sel = this.displaySelect_.options[this.displaySelect_.selectedIndex];
    if (!sel) {
      this.selectedAndConnectedDisplayAddress_ = undefined;
      return;
    }

    chrome.bluetooth.getDevice(sel.id, (display) => {
      // Record metrics if the display is connected for the first time either
      // via a click of the Connect button or re-connection by selection via the
      // select.
      if (display.connected) {
        if (this.selectedAndConnectedDisplayAddress_ != sel.id) {
          this.selectedAndConnectedDisplayAddress_ = sel.id;
          chrome.metricsPrivate.recordUserAction(
              BluetoothBrailleDisplayUI.CONNECTED_METRIC_NAME_);
        }
      } else {
        // The display is no longer connected.
        if (this.selectedAndConnectedDisplayAddress_ == sel.id) {
          this.selectedAndConnectedDisplayAddress_ = undefined;
        }
      }

      var connectOrDisconnect =
          this.controls_.querySelector('#connectOrDisconnect');
      connectOrDisconnect.disabled = display.connecting;
      this.displaySelect_.disabled = display.connecting;
      connectOrDisconnect.textContent = Msgs.getMsg(
          display.connecting ?
              'options_bluetooth_braille_display_connecting' :
              (display.connected ?
                   'options_bluetooth_braille_display_disconnect' :
                   'options_bluetooth_braille_display_connect'));
      connectOrDisconnect.onclick = function(savedDisplay, evt) {
        if (savedDisplay.connected) {
          this.manager_.disconnect(savedDisplay);
        } else {
          this.manager_.connect(savedDisplay);
        }
      }.bind(this, display);

      var forget = this.controls_.querySelector('#forget');
      forget.disabled = !display.paired || display.connecting;
      forget.onclick = function(savedDisplay) {
        this.manager_.forget(savedDisplay);
      }.bind(this, display);
    });
  }
};

/** @private {string} */
BluetoothBrailleDisplayUI.CONNECTED_METRIC_NAME_ =
    'Accessibility.ChromeVox.BluetoothBrailleDisplayConnectedButtonClick';
