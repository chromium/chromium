// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('login', function() {
  /** @const */ var CALLBACK_USER_ACTED = 'userActed';

  var OobeScreenBehavior = {
    properties: {
      name: String
    },

    /**
     * The login.Screen which is hosting |this|.
     */
    screen_: null,

    /**
     * Called when the screen is being registered.
     */
    initialize: function() {},

    ready: function() {
      if (this.decorate_) {
        this.initialize();
      } else {
        this.ready_ = true;
      }
    },

    userActed: function(e) {
      this.send(
          CALLBACK_USER_ACTED,
          e.detail.sourceEvent.target.getAttribute('action'));
    },

    i18n: function(args) {
      if (!(args instanceof Array))
        args = [args];
      args[0] = 'login_' + this.name + '_' + args[0];
      return loadTimeData.getStringF.apply(loadTimeData, args);
    },

    /**
     * Called by login.Screen when the screen is beeing registered.
     */
    decorate: function(screen) {
      this.screen_ = screen;
      var self = this;
      if (this.ready_) {
        this.initialize();
      } else {
        this.decorate_ = true;
      }
    },

    /**
     * @final
     */
    send: function() {
      return this.sendImpl_.apply(this, arguments);
    },

    /**
     * @override
     * @final
     */
    querySelector: function() {
      return this.querySelectorImpl_.apply(this, arguments);
    },

    /**
     * @override
     * @final
     */
    querySelectorAll: function() {
      return this.querySelectorAllImpl_.apply(this, arguments);
    },

    /**
     * See login.Screen.send.
     * @private
     */
    sendImpl_: function() {
      return this.screen_.send.apply(this.screen_, arguments);
    },

    /**
     * Calls |querySelector| method of the shadow dom and returns the result.
     * @private
     */
    querySelectorImpl_: function(selector) {
      return this.shadowRoot.querySelector(selector);
    },


    /**
     * Calls standart |querySelectorAll| method of the shadow dom and returns
     * the result converted to Array.
     * @private
     */
    querySelectorAllImpl_: function(selector) {
      var list = this.shadowRoot.querySelectorAll(selector);
      return Array.prototype.slice.call(list);
    },

    /**
     * If |value| is the value of some property of |this| returns property's
     * name. Otherwise returns empty string.
     * @private
     */
    getPropertyNameOf_: function(value) {
      for (var key in this)
        if (this[key] === value)
          return key;
      return '';
    }
  };

  return {OobeScreenBehavior: OobeScreenBehavior};
});
