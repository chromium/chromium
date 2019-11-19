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
 * Creates the Dialog view controller.
 * @extends {cca.views.View}
 * @constructor
 * @param {string} viewId Root element id of dialog view.
 */
cca.views.Dialog = function(viewId) {
  cca.views.View.call(this, viewId, true);

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.positiveButton_ =
      document.querySelector(`${viewId} .dialog-positive-button`);

  /**
   * @type {!HTMLButtonElement}
   * @private
   */
  this.negativeButton_ =
      document.querySelector(`${viewId} .dialog-negative-button`);

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.messageHolder_ = document.querySelector(`${viewId} .dialog-msg-holder`);

  // End of properties, seal the object.
  Object.seal(this);

  this.positiveButton_.addEventListener('click', () => this.leave(true));
  if (this.negativeButton_) {
    this.negativeButton_.addEventListener('click', () => this.leave());
  }
};

cca.views.Dialog.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @param {string} message Message of the dialog.
 * @param {boolean} cancellable Whether the dialog is cancellable.
 * @override
 */
cca.views.Dialog.prototype.entering = function({message, cancellable} = {}) {
  if (this.messageHolder_ && message) {
    this.messageHolder_.textContent = message;
  }
  if (this.negativeButton_) {
    this.negativeButton_.hidden = !cancellable;
  }
};

/**
 * @override
 */
cca.views.Dialog.prototype.focus = function() {
  this.positiveButton_.focus();
};
