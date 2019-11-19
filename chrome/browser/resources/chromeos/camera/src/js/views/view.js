// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Base controller of a view for views' navigation sessions (cca.nav).
 * @param {string} selector Selector text of the view's root element.
 * @param {boolean=} dismissByEsc Enable dismissible by Esc-key.
 * @param {boolean=} dismissByBkgndClick Enable dismissible by background-click.
 * @constructor
 */
cca.views.View = function(
    selector, dismissByEsc = false, dismissByBkgndClick = false) {
  /**
   * @type {!HTMLElement}
   * @protected
   */
  this.rootElement_ =
      /** @type {!HTMLElement} */ (document.querySelector(selector));

  /**
   * @type {Promise<*>}
   * @private
   */
  this.session_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.dismissByEsc_ = dismissByEsc;

  if (dismissByBkgndClick) {
    this.rootElement_.addEventListener(
        'click',
        (event) =>
            event.target === this.rootElement_ && this.leave({bkgnd: true}));
  }
};

cca.views.View.prototype = {
  get root() {
    return this.rootElement_;
  },
};

/**
 * Hook of the subclass for handling the key.
 * @param {string} key Key to be handled.
 * @return {boolean} Whether the key has been handled or not.
 */
cca.views.View.prototype.handlingKey = function(key) {
  return false;
};

/**
 * Handles the pressed key.
 * @param {string} key Key to be handled.
 * @return {boolean} Whether the key has been handled or not.
 */
cca.views.View.prototype.onKeyPressed = function(key) {
  if (this.handlingKey(key)) {
    return true;
  } else if (key === 'Ctrl-V') {
    const {version, version_name: versionName} = chrome.runtime.getManifest();
    cca.toast.show(versionName || version);
    return true;
  } else if (this.dismissByEsc_ && key === 'Escape') {
    this.leave();
    return true;
  }
  return false;
};

/**
 * Focuses the default element on the view if applicable.
 */
cca.views.View.prototype.focus = function() {
};

/**
 * Layouts the view.
 */
cca.views.View.prototype.layout = function() {
};

/**
 * Hook of the subclass for entering the view.
 * @param {...*} args Optional rest parameters for entering the view.
 */
cca.views.View.prototype.entering = function(...args) {
};

/**
 * Enters the view.
 * @param {...*} args Optional rest parameters for entering the view.
 * @return {!Promise<*>} Promise for the navigation session.
 */
cca.views.View.prototype.enter = function(...args) {
  // The session is started by entering the view and ended by leaving the view.
  if (!this.session_) {
    var end;
    this.session_ = new Promise((resolve) => {
      end = resolve;
    });
    this.session_.end = (result) => {
      end(result);
    };
  }
  this.entering(...args);
  return this.session_;
};

/**
 * Hook of the subclass for leaving the view.
 * @param {*=} condition Optional condition for leaving the view.
 * @return {boolean} Whether able to leaving the view or not.
 */
cca.views.View.prototype.leaving = function(condition) {
  return true;
};

/**
 * Leaves the view.
 * @param {*=} condition Optional condition for leaving the view and also as the
 *     result for the ended session.
 * @return {boolean} Whether able to leaving the view or not.
 */
cca.views.View.prototype.leave = function(condition) {
  if (this.session_ && this.leaving(condition)) {
    this.session_.end(condition);
    this.session_ = null;
    return true;
  }
  return false;
};
