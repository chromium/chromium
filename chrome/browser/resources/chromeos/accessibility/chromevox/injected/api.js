// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Public APIs to enable web applications to communicate
 * with ChromeVox.
 */
if (typeof (goog) !== 'undefined' && goog.provide) {
goog.provide('cvox.Api');
}


(function() {
/*
 * Private data and methods.
 */

/**
 * The name of the port between the content script and background page.
 * @type {string}
 * @const
 */
const PORT_NAME = 'cvox.Port';

/**
 * The name of the message between the page and content script that sets
 * up the bidirectional port between them.
 * @type {string}
 * @const
 */
const PORT_SETUP_MSG = 'cvox.PortSetup';

/**
 * The message between content script and the page that indicates the
 * connection to the background page has been lost.
 * @type {string}
 * @const
 */
const DISCONNECT_MSG = 'cvox.Disconnect';

/**
 * The channel between the page and content script.
 * @type {MessageChannel}
 */
let channel;

/**
 * Tracks whether or not the ChromeVox API should be considered active.
 * @type {boolean}
 */
let isActive = false;

/**
 * The next id to use for async callbacks.
 * @type {number}
 */
let nextCallbackId = 1;

/**
 * Map from callback ID to callback function.
 * @type {Object<number, function(*)>}
 */
const callbackMap = {};

/**
 * Internal function to connect to the content script.
 */
function connect_() {
  if (channel) {
    // If there is already an existing channel, close the existing ports.
    channel.port1.close();
    channel.port2.close();
    channel = null;
  }

  channel = new MessageChannel();
  channel.port1.onmessage = function(event) {
    if (event.data === DISCONNECT_MSG) {
      channel = null;
      const event = document.createEvent('UIEvents');
      event.initEvent('chromeVoxUnloaded', true, false);
      document.dispatchEvent(event);
      return;
    }
    try {
      const message = JSON.parse(event.data);
      if (message['id'] && callbackMap[message['id']]) {
        callbackMap[message['id']](message);
        delete callbackMap[message['id']];
      }
    } catch (e) {
    }
  };
  window.postMessage(
      PORT_SETUP_MSG, '*' /* target origin */, [channel.port2] /* transfer */);
}

/**
 * Internal function to send a message to the content script and
 * call a callback with the response.
 * @param {Object} message A serializable message.
 * @param {function(*)} callback A callback that will be called
 *     with the response message.
 */
function callAsync_(message, callback) {
  const id = nextCallbackId;
  nextCallbackId++;
  if (message['args'] === undefined) {
    message['args'] = [];
  }
  message['args'] = [id].concat(message['args']);
  callbackMap[id] = callback;
  channel.port1.postMessage(JSON.stringify(message));
}

/**
 * Wraps callAsync_ for sending speak requests.
 * @param {Object} message A serializable message.
 * @param {Object=} properties Speech properties to use for this utterance.
 * @private
 */
function callSpeakAsync_(message, properties) {
  let callback = null;
  /* Use the user supplied callback as callAsync_'s callback. */
  if (properties && properties['endCallback']) {
    callback = properties['endCallback'];
  }
  callAsync_(message, callback);
}

/**
 * Gets an object given a dotted namespace object path.
 * @param {string} path
 * @return {*}
 */
function getObjectByName(path) {
  const pieces = path.split('.');
  let resolved = window;
  for (let i = 0; i < pieces.length; i++) {
    resolved = resolved[pieces[i]];
    if (!resolved) {
      return null;
    }
  }
  return resolved;
}


/**
 * Maybe enable MathJaX support.
 */
function maybeEnableMathJaX() {
  if (!getObjectByName('MathJax.Hub.Register.LoadHook') ||
      !getObjectByName('MathJax.Ajax.Require')) {
    return;
  }

  MathJax.Hub.Register.LoadHook('[a11y]/explorer.js', function() {
    // |explorer| is an object instance, so we get it to call an instance
    // |method.
    const explorer = getObjectByName('MathJax.Extension.explorer');
    if (explorer.Enable) {
      explorer.Enable(true, true);
    }
  });
  MathJax.Ajax.Require('[a11y]/explorer.js');
}


/*
 * Public API.
 */

if (!window['cvox']) {
  window['cvox'] = {};
}
const cvox = window.cvox;


/**
 * @constructor
 */
cvox.Api = function() {};

/**
 * Internal-only function, only to be called by the content script.
 * Enables the API and connects to the content script.
 */
cvox.Api.internalEnable = function() {
  isActive = true;
  connect_();
  const event = document.createEvent('UIEvents');
  event.initEvent('chromeVoxLoaded', true, false);
  document.dispatchEvent(event);
};

/**
 * Returns true if ChromeVox is currently running. If the API is available
 * in the JavaScript namespace but this method returns false, it means that
 * the user has (temporarily) disabled ChromeVox.
 *
 * You can listen for the 'chromeVoxLoaded' event to be notified when
 * ChromeVox is loaded.
 *
 * @return {boolean} True if ChromeVox is currently active.
 */
cvox.Api.isChromeVoxActive = function() {
  return Boolean(channel);
};

/**
 * Speaks the given string using the specified queueMode and properties.
 *
 * @param {string} textString The string of text to be spoken.
 * @param {number=} queueMode Valid modes are 0 for flush; 1 for queue.
 * @param {Object=} properties Speech properties to use for this utterance.
 */
cvox.Api.speak = function(textString, queueMode, properties) {
  if (!cvox.Api.isChromeVoxActive()) {
    return;
  }

  const message = {'cmd': 'speak', 'args': [textString, queueMode, properties]};
  callSpeakAsync_(message, properties);
};

/**
 * This method is kept to keep Docs from throwing an error.
 */
cvox.Api.stop = function() {};

cvox.Api.internalEnable();
})();
