// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemBehavior} from './item_behavior.js';
import {navigation, Page} from './navigation_helper.js';

/**
 * @return {!Promise} A signal that the document is ready. Need to wait for
 *     this, otherwise the custom ExtensionOptions element might not have been
 *     registered yet.
 */
function whenDocumentReady() {
  if (document.readyState == 'complete') {
    return Promise.resolve();
  }

  return new Promise(function(resolve) {
    document.addEventListener('readystatechange', function f() {
      if (document.readyState == 'complete') {
        document.removeEventListener('readystatechange', f);
        resolve();
      }
    });
  });
}

// The minimum width in pixels for the options dialog.
export const OptionsDialogMinWidth = 400;

// The maximum height in pixels for the options dialog.
export const OptionsDialogMaxHeight = 640;

Polymer({
  is: 'extensions-options-dialog',

  _template: html`{__html_template__}`,

  behaviors: [ItemBehavior],

  properties: {
    /** @private {Object} */
    extensionOptions_: Object,

    /** @private {chrome.developerPrivate.ExtensionInfo} */
    data_: Object,
  },

  /** @private {?Function} */
  boundUpdateDialogSize_: null,

  /** @private {?{height: number, width: number}} */
  preferredSize_: null,

  get open() {
    return this.$.dialog.open;
  },

  /**
   * Resizes the dialog to the width/height stored in |preferredSize_|, taking
   * into account the window width/height.
   * @private
   */
  updateDialogSize_: function() {
    const headerHeight = this.$.body.offsetTop;
    const maxHeight =
        Math.min(0.9 * window.innerHeight, OptionsDialogMaxHeight);
    const effectiveHeight =
        Math.min(maxHeight, headerHeight + this.preferredSize_.height);
    const effectiveWidth =
        Math.max(OptionsDialogMinWidth, this.preferredSize_.width);

    this.$.dialog.style.setProperty('--dialog-height', `${effectiveHeight}px`);
    this.$.dialog.style.setProperty('--dialog-width', `${effectiveWidth}px`);
    this.$.dialog.style.setProperty('--dialog-opacity', '1');
  },

  /** @param {chrome.developerPrivate.ExtensionInfo} data */
  show: function(data) {
    this.data_ = data;
    whenDocumentReady().then(() => {
      if (!this.extensionOptions_) {
        this.extensionOptions_ = document.createElement('ExtensionOptions');
      }
      this.extensionOptions_.extension = this.data_.id;
      this.extensionOptions_.onclose = () => this.$.dialog.close();

      const boundUpdateDialogSize = this.updateDialogSize_.bind(this);
      this.boundUpdateDialogSize_ = boundUpdateDialogSize;
      this.extensionOptions_.onpreferredsizechanged = e => {
        if (!this.$.dialog.open) {
          this.$.dialog.showModal();
        }
        this.preferredSize_ = e;
        this.debounce('updateDialogSize_', boundUpdateDialogSize, 50);
      };

      // Add a 'resize' such that the dialog is resized when window size
      // changes.
      window.addEventListener('resize', this.boundUpdateDialogSize_);
      this.$.body.appendChild(/** @type {Node} */ (this.extensionOptions_));
    });
  },

  /** @private */
  onClose_: function() {
    this.extensionOptions_.onpreferredsizechanged = null;

    if (this.boundUpdateDialogSize_) {
      window.removeEventListener('resize', this.boundUpdateDialogSize_);
      this.boundUpdateDialogSize_ = null;
    }

    const currentPage = navigation.getCurrentPage();
    // We update the page when the options dialog closes, but only if we're
    // still on the details page. We could be on a different page if the
    // user hit back while the options dialog was visible; in that case, the
    // new page is already correct.
    if (currentPage && currentPage.page == Page.DETAILS) {
      // This will update the currentPage_ and the NavigationHelper; since
      // the active page is already the details page, no main page
      // transition occurs.
      navigation.navigateTo(
          {page: Page.DETAILS, extensionId: currentPage.extensionId});
    }
  },
});
