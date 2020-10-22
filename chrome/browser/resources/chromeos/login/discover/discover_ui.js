// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying Discover UI.
 */
function initializeDiscoverAPI() {
  let discoverCallbacks = {};

  window.discoverSendImpl = (message, callback, parameters) => {
    assert(message.startsWith('discover.'));
    let fullParameters = [];
    // Callback Id should be random to prevent triggering of incorrect
    // callbacks if Id get screwed.
    if (callback) {
      let callbackId;
      for (let i = 0; i < 10; ++i) {
        callbackId =
            String(Math.floor(Math.random() * 2147483647));  // 2^31 - 1
        if (callbackId && !(callbackId in discoverCallbacks))
          break;
      }
      assert(!(callbackId in discoverCallbacks));
      discoverCallbacks[callbackId] = callback;
      fullParameters.push(callbackId);
    }

    if (parameters && parameters.length)
      fullParameters = fullParameters.concat(parameters);

    if (fullParameters.length) {
      chrome.send(message, fullParameters);
    } else {
      chrome.send(message);
    }
  };

  window.discoverReturn = (callbackId, value) => {
    assert(callbackId in discoverCallbacks);
    let callback = discoverCallbacks[callbackId];
    assert(delete (discoverCallbacks[callbackId]));
    callback.call(null, value);
  };
}

{
  Polymer({
    is: 'discover-ui',

    behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

    properties: {
      /**
       * When this flag is true, Discover UI is displayed as part of FirstRun
       * UI.
       */
      firstRun: {
        type: Boolean,
        value: false,
      },
    },

    updateLocalizedContent() {
      this.i18nUpdateLocale();
      this.propagateUpdateLocalizedContent('.card');
      this.propagateUpdateLocalizedContent('.module');
    },

    /*
     * Enumerates modules, attaches common event handlers.
     * @override
     */
    attached() {
      initializeDiscoverAPI();
      // Initialize modules event handlers.
      let modules = Polymer.dom(this.root).querySelectorAll('.module');
      for (let i = 0; i < modules.length; ++i) {
        let module = modules[i];
        let handlerBack =
            this.showModule_.bind(this, module.getAttribute('module'));
        let handlerContinue = this.end_.bind(this);
        module.addEventListener('module-back', handlerBack);
        module.addEventListener('module-continue', handlerContinue);
      }
    },

    /*
     * Overridden from OobeDialogHostBehavior.
     * @override
     */
    onBeforeShow() {
      this.propagateOnBeforeShow('.module');
      this.showModule_('pinSetup');
    },

    /*
     * Hides all modules.
     * @private
     */
    hideAll_() {
      let modules = Polymer.dom(this.root).querySelectorAll('.module');
      for (let module of modules)
        module.hidden = true;
    },

    /*
     * Shows module identified by |moduleId|.
     * @param {string} moduleId Module name (shared between C++ and JS).
     * @private
     */
    showModule_(moduleId) {
      let module = Polymer.dom(this.root).querySelector(
          '.module[module="' + moduleId + '"]');
      if (module) {
        this.hideAll_();
        cr.ui.login.invokePolymerMethod(module, 'onBeforeShow');
        module.hidden = false;
        module.show();
      } else {
        console.error('Module "' + moduleId + '" not found!');
      }
    },

    /*
     * @param {!Event} event The onInput event that the function is handling.
     * @private
     */
    onCardClick_(event) {
      let module = event.target.getAttribute('module');
      this.showModule_(module);
    },

    /*
     * Fires signal that Discover app has ended.
     * @private
     */
    end_() {
      this.fire('discover-done');
    },

    /*
     * Starts linear navigation flow.
     * @private
     */
    startLinearFlow_() {
      this.showModule_(
          Polymer.dom(this.root).querySelector('.module').getAttribute(
              'module'));
    },
  });
}
