// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implentation of ChromeVox's public API.
 *
 */

goog.provide('ApiImplementation');

goog.require('ChromeVox');
goog.require('ExtensionBridge');
goog.require('ScriptInstaller');

ApiImplementation = class {
  constructor() {}

  /**
   * Inject the API into the page and set up communication with it.
   * @param {function()=} opt_onload A function called when the script is
   *     loaded.
   */
  static init(opt_onload) {
    window.addEventListener('message', ApiImplementation.portSetup, true);
    const scripts =
        [window.chrome.extension.getURL('chromevox/injected/api.js')];

    const didInstall =
        ScriptInstaller.installScript(scripts, 'cvoxapi', opt_onload);
    if (!didInstall) {
      console.error('Unable to install api scripts');
    }

    ExtensionBridge.addDisconnectListener(function() {
      ApiImplementation.port.postMessage(ApiImplementation.DISCONNECT_MSG);
      ScriptInstaller.uninstallScript('cvoxapi');
    });
  }

  /**
   * This method is called when the content script receives a message from
   * the page.
   * @param {Event} event The DOM event with the message data.
   * @return {boolean} True if default event processing should continue.
   */
  static portSetup(event) {
    if (event.data === 'cvox.PortSetup') {
      ApiImplementation.port = event.ports[0];
      ApiImplementation.port.onmessage = function(event) {
        ApiImplementation.dispatchApiMessage(JSON.parse(event.data));
      };

      // Stop propagation since it was our message.
      event.stopPropagation();
      return false;
    }
    return true;
  }

  /**
   * Call the appropriate API function given a message from the page.
   * @param {*} message The message.
   */
  static dispatchApiMessage(message) {
    let method;
    switch (message['cmd']) {
      case 'speak':
        method = ApiImplementation.speak;
        break;
        break;
    }
    if (!method) {
      throw 'Unknown API call: ' + message['cmd'];
    }

    method.apply(ApiImplementation, message['args']);
  }

  /**
   * Speaks the given string using the specified queueMode and properties.
   *
   * @param {number} callbackId The callback Id.
   * @param {string} textString The string of text to be spoken.
   * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
   * @param {Object=} properties Speech properties to use for this utterance.
   */
  static speak(callbackId, textString, queueMode, properties) {
    if (!properties) {
      properties = {};
    }
    setupEndCallback_(properties, callbackId);
    const message = {
      'target': 'TTS',
      'action': 'speak',
      'text': textString,
      queueMode,
      properties
    };

    ExtensionBridge.send(message);
  }
};

/**
 * The message between content script and the page that indicates the
 * connection to the background page has been lost.
 * @type {string}
 * @const
 */
ApiImplementation.DISCONNECT_MSG = 'Disconnect';


/**
 * Sets endCallback in properties to call callbackId's function.
 * @param {Object} properties Speech properties to use for this utterance.
 * @param {number} callbackId The callback Id.
 * @private
 */
function setupEndCallback_(properties, callbackId) {
  const endCallback = function() {
    ApiImplementation.port.postMessage(JSON.stringify({'id': callbackId}));
  };
  if (properties) {
    properties['endCallback'] = endCallback;
  }
}
