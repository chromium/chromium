// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import '../strings.m.js';
import './throbber_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {DestinationStore} from '../data/destination_store.js';

/**
 * @fileoverview PrintPreviewProvisionalDestinationResolver
 * This class is a dialog for resolving provisional destinations. Provisional
 * destinations are extension controlled destinations that need access to a USB
 * device and have not yet been granted access by the user. Destinations are
 * resolved when the user confirms they wish to grant access and the handler
 * has successfully granted access.
 */

/**
 * States that the provisional destination resolver can be in.
 * @enum {string}
 */
const ResolverState = {
  INITIAL: 'INITIAL',
  ACTIVE: 'ACTIVE',
  GRANTING_PERMISSION: 'GRANTING_PERMISSION',
  ERROR: 'ERROR',
  DONE: 'DONE'
};

Polymer({
  is: 'print-preview-provisional-destination-resolver',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?DestinationStore} */
    destinationStore: Object,

    /** @private {?Destination} */
    destination_: {
      type: Object,
      value: null,
    },

    /** @private {!ResolverState} */
    state_: {
      type: String,
      value: ResolverState.INITIAL,
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /**
   * Promise resolver for promise returned by {@code this.resolveDestination}.
   * @private {?PromiseResolver<!Destination>}
   */
  promiseResolver_: null,

  /**
   * @param {!Destination} destination The destination this
   *     dialog is needed to resolve.
   * @return {!Promise} Promise that is resolved when the destination has been
   *     resolved.
   */
  resolveDestination: function(destination) {
    this.state_ = ResolverState.ACTIVE;
    this.destination_ = destination;
    this.$.dialog.showModal();
    const icon = this.$$('.extension-icon');
    icon.style.backgroundImage = '-webkit-image-set(' +
        'url(chrome://extension-icon/' + this.destination_.extensionId +
        '/24/1) 1x,' +
        'url(chrome://extension-icon/' + this.destination_.extensionId +
        '/48/1) 2x)';
    this.promiseResolver_ = new PromiseResolver();
    return this.promiseResolver_.promise;
  },

  /**
   * Handler for click on OK button. It attempts to resolve the destination.
   * If successful, promiseResolver_.promise is resolved with the
   * resolved destination and the dialog closes.
   * @private
   */
  startResolveDestination_: function() {
    assert(
        this.state_ == ResolverState.ACTIVE,
        'Invalid state in request grant permission');

    this.state_ = ResolverState.GRANTING_PERMISSION;
    const destination =
        /** @type {!Destination} */ (this.destination_);
    this.destinationStore.resolveProvisionalDestination(destination)
        .then(
            /** @param {?Destination} resolvedDestination */
            (resolvedDestination) => {
              if (this.state_ != ResolverState.GRANTING_PERMISSION) {
                return;
              }

              if (destination.id != this.destination_.id) {
                return;
              }

              if (resolvedDestination) {
                this.state_ = ResolverState.DONE;
                this.promiseResolver_.resolve(resolvedDestination);
                this.promiseResolver_ = null;
                this.$.dialog.close();
              } else {
                this.state_ = ResolverState.ERROR;
              }
            });
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_: function(e) {
    e.stopPropagation();
    if (e.key == 'Escape') {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  },

  /** @private */
  onCancelClick_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onCancel_: function() {
    this.promiseResolver_.reject();
    this.state_ = ResolverState.INITIAL;
  },

  /**
   * @return {string} The USB permission message to display.
   * @private
   */
  getPermissionMessage_: function() {
    return this.state_ == ResolverState.ERROR ?
        this.i18n(
            'resolveExtensionUSBErrorMessage',
            this.destination_.extensionName) :
        this.i18n('resolveExtensionUSBPermissionMessage');
  },

  /**
   * @return {boolean} Whether the resolver is in the ERROR state.
   * @private
   */
  isInErrorState_: function() {
    return this.state_ == ResolverState.ERROR;
  },

  /**
   * @return {boolean} Whether the resolver is in the ACTIVE state.
   * @private
   */
  isInActiveState_: function() {
    return this.state_ == ResolverState.ACTIVE;
  },

  /**
   * @return {string} 'throbber' if the resolver is in the GRANTING_PERMISSION
   *     state, empty otherwise.
   */
  getThrobberClass_: function() {
    return this.state_ == ResolverState.GRANTING_PERMISSION ? 'throbber' : '';
  },
});
