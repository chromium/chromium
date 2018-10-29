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
    // Callback Id should be random to prevent triggering of incorrect
    // callbacks if Id get screwed.
    let callbackId;
    if (callback) {
      for (let i = 0; i < 10; ++i) {
        callbackId =
            String(Math.floor(Math.random() * 2147483647));  // 2^31 - 1
        if (callbackId && !(callbackId in discoverCallbacks))
          break;
      }
      assert(!(callbackId in discoverCallbacks));
      discoverCallbacks[callbackId] = callback;
    }
    chrome.send(message, [callbackId].concat(parameters));
  };

  window.discoverReturn = (callbackId, value) => {
    assert(callbackId in discoverCallbacks);
    let callback = discoverCallbacks[callbackId];
    assert(delete (discoverCallbacks[callbackId]));
    callback.call(null, value);
  };
}

{
  const DISCOVER_WELCOME_MODULE = 'discoverWelcome';

  Polymer({
    is: 'discover-ui',

    behaviors: [I18nBehavior, OobeDialogHostBehavior],

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

    updateLocalizedContent: function() {
      this.i18nUpdateLocale();
      this.propagateUpdateLocalizedContent('.card');
      this.propagateUpdateLocalizedContent('#discoverWelcome');
      this.propagateUpdateLocalizedContent('.module');
    },

    /*
     * Enumerates modules, attaches common event handlers.
     * @override
     */
    attached: function() {
      initializeDiscoverAPI();
      // Initialize modules event handlers.
      let modules = Polymer.dom(this.root).querySelectorAll('.module');
      for (let i = 0; i < modules.length; ++i) {
        let module = modules[i];
        let handlerBack = this.showModule_.bind(
            this,
            (i > 0 ? modules[i - 1].getAttribute('module') :
                     'discoverWelcome'));
        let handlerContinue;
        if (i < modules.length - 1) {
          handlerContinue = this.showModule_.bind(
              this, modules[i + 1].getAttribute('module'));
        } else {
          handlerContinue = this.end_.bind(this);
        }
        module.addEventListener('module-back', handlerBack);
        module.addEventListener('module-continue', handlerContinue);
      }
    },

    /*
     * Overridden from OobeDialogHostBehavior.
     * @override
     */
    onBeforeShow: function() {
      OobeDialogHostBehavior.onBeforeShow.call(this);
      this.propagateFullScreenMode('#discoverWelcome');
      this.propagateFullScreenMode('.module');

      if (this.firstRun) {
        this.showModule_('pinSetup');
      } else {
        this.showModule_(DISCOVER_WELCOME_MODULE);
      }
    },

    /*
     * Hides all modules.
     * @private
     */
    hideAll_: function() {
      this.$.discoverWelcome.hidden = true;
      let modules = Polymer.dom(this.root).querySelectorAll('.module');
      for (let module of modules)
        module.hidden = true;
    },

    /*
     * Shows module identified by |moduleId|.
     * @param {string} moduleId Module name (shared between C++ and JS).
     * @private
     */
    showModule_: function(moduleId) {
      let module;
      if (moduleId === DISCOVER_WELCOME_MODULE) {
        module = this.$.discoverWelcome;
      } else {
        module = Polymer.dom(this.root).querySelector(
            '.module[module="' + moduleId + '"]');
      }
      if (module) {
        this.hideAll_();
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
    onCardClick_: function(event) {
      let module = event.target.getAttribute('module');
      this.showModule_(module);
    },

    /*
     * Fires signal that Discover app has ended.
     * @private
     */
    end_: function() {
      this.fire('discover-done');
    },

    /*
     * Starts linear navigation flow.
     * @private
     */
    startLinearFlow_: function() {
      this.showModule_(
          Polymer.dom(this.root).querySelector('.module').getAttribute(
              'module'));
    },
  });
}
