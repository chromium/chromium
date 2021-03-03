// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-page-template is used as a template for pages. It
 * provide a consistent setup for all pages with title, sub-title, body slot
 * and button options.
 */
Polymer({
  is: 'nearby-page-template',

  properties: {
    /** @type {?string} */
    title: {
      type: String,
    },

    /** @type {?string} */
    subTitle: {
      type: String,
    },

    /**
     * Alternate subtitle for screen readers. If not falsey, then the
     * #pageSubTitle is aria-hidden and the #a11yAnnouncedPageSubTitle is
     * rendered on screen readers instead. Changes to this value will result in
     * aria-live announcements.
     * @type {?string}
     * */
    a11yAnnouncedSubTitle: {
      type: String,
    },

    /**
     * Text to show on the action button. If either this is falsey, or if
     * |closeOnly| is true, then the action button is hidden.
     * @type {?string}
     * */
    actionButtonLabel: {
      type: String,
    },

    /** @type {string} */
    actionButtonEventName: {type: String, value: 'action'},

    actionDisabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Text to show on the cancel button. If either this is falsey, or if
     * |closeOnly| is true, then the cancel button is hidden.
     * @type {?string}
     * */
    cancelButtonLabel: {
      type: String,
    },

    /** @type {string} */
    cancelButtonEventName: {
      type: String,
      value: 'cancel',
    },

    /**
     * Text to show on the utility button. If either this is falsey, or if
     * |closeOnly| is true, then the utility button is hidden.
     * @type {?string}
     * */
    utilityButtonLabel: {
      type: String,
    },

    /**
     * When true, shows the open-in-new icon to the left of the button label.
     * @type {boolean}
     * */
    utilityButtonOpenInNew: {
      type: Boolean,
      value: false,
    },

    /** @type {string} */
    utilityButtonEventName: {
      type: String,
      value: 'utility',
    },

    /**
     * When true, hide all other buttons and show a close button.
     * @type {boolean}
     * */
    closeOnly: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onActionClick_() {
    this.fire(this.actionButtonEventName);
  },

  /** @private */
  onCancelClick_() {
    this.fire(this.cancelButtonEventName);
  },

  /** @private */
  onUtilityClick_() {
    this.fire(this.utilityButtonEventName);
  },

  /** @private */
  onCloseClick_() {
    this.fire('close');
  },

  /**
   * @return {string} aria-labelledby ids for the dialog
   * @private
   */
  getDialogAriaLabelledBy_() {
    let labelIds = 'pageTitle';
    if (!this.a11yAnnouncedSubTitle) {
      labelIds += ' pageSubTitle';
    }
    return labelIds;
  },

  /**
   * @return {string|undefined} aria-hidden value for the #subTitle div.
   *     'true' or undefined.
   * @private
   */
  getSubTitleAriaHidden_() {
    if (this.a11yAnnouncedSubTitle) {
      return 'true';
    }
    return undefined;
  },
});
