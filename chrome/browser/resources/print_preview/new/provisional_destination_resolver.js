// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PrintPreviewProvisionalDestinationResolver
 * This class is a dialog for resolving provisional destinations. Provisional
 * destinations are extension controlled destinations that need access to a USB
 * device and have not yet been granted access by the user. Destinations are
 * resolved when the user confirms they wish to grant access and the handler
 * has successfully granted access.
 */

cr.exportPath('print_preview_new');

/**
 * States that the provisional destination resolver can be in.
 * @enum {string}
 */
print_preview_new.ResolverState = {
  INITIAL: 'INITIAL',
  ACTIVE: 'ACTIVE',
  GRANTING_PERMISSION: 'GRANTING_PERMISSION',
  ERROR: 'ERROR',
  DONE: 'DONE'
};

Polymer({
  is: 'print-preview-provisional-destination-resolver',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?print_preview.DestinationStore} */
    destinationStore: Object,

    /** @private {?print_preview.Destination} */
    destination_: {
      type: Object,
      value: null,
    },

    /** @private {!print_preview_new.ResolverState} */
    state_: {
      type: String,
      value: print_preview_new.ResolverState.INITIAL,
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /**
   * Promise resolver for promise returned by {@code this.resolveDestination}.
   * @private {?PromiseResolver<!print_preview.Destination>}
   */
  promiseResolver_: null,

  /**
   * @param {!print_preview.Destination} destination The destination this
   *     dialog is needed to resolve.
   * @return {!Promise} Promise that is resolved when the destination has been
   *     resolved.
   */
  resolveDestination: function(destination) {
    this.state_ = print_preview_new.ResolverState.ACTIVE;
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
        this.state_ == print_preview_new.ResolverState.ACTIVE,
        'Invalid state in request grant permission');

    this.state_ = print_preview_new.ResolverState.GRANTING_PERMISSION;
    const destination =
        /** @type {!print_preview.Destination} */ (this.destination_);
    this.destinationStore.resolveProvisionalDestination(destination)
        .then(
            /** @param {?print_preview.Destination} resolvedDestination */
            (resolvedDestination) => {
              if (this.state_ !=
                  print_preview_new.ResolverState.GRANTING_PERMISSION) {
                return;
              }

              if (destination.id != this.destination_.id)
                return;

              if (resolvedDestination) {
                this.state_ = print_preview_new.ResolverState.DONE;
                this.promiseResolver_.resolve(resolvedDestination);
                this.promiseResolver_ = null;
                this.$.dialog.close();
              } else {
                this.state_ = print_preview_new.ResolverState.ERROR;
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
    this.state_ = print_preview_new.ResolverState.INITIAL;
  },

  /**
   * @return {string} The USB permission message to display.
   * @private
   */
  getPermissionMessage_: function() {
    return this.state_ == print_preview_new.ResolverState.ERROR ?
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
    return this.state_ == print_preview_new.ResolverState.ERROR;
  },

  /**
   * @return {boolean} Whether the resolver is in the ACTIVE state.
   * @private
   */
  isInActiveState_: function() {
    return this.state_ == print_preview_new.ResolverState.ACTIVE;
  },

  /**
   * @return {string} 'throbber' if the resolver is in the GRANTING_PERMISSION
   *     state, empty otherwise.
   */
  getThrobberClass_: function() {
    return this.state_ == print_preview_new.ResolverState.GRANTING_PERMISSION ?
        'throbber' :
        '';
  },
});
