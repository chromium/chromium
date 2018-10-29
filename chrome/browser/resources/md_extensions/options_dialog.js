// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @return {!Promise} A signal that the document is ready. Need to wait for
   *     this, otherwise the custom ExtensionOptions element might not have been
   *     registered yet.
   */
  function whenDocumentReady() {
    if (document.readyState == 'complete')
      return Promise.resolve();

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
  const MIN_WIDTH = 400;

  // The maximum height in pixels for the options dialog.
  const MAX_HEIGHT = 640;

  const OptionsDialog = Polymer({
    is: 'extensions-options-dialog',

    behaviors: [extensions.ItemBehavior],

    properties: {
      /** @private {Object} */
      extensionOptions_: Object,

      /** @private {chrome.developerPrivate.ExtensionInfo} */
      data_: Object,
    },

    /** @private {?Function} */
    boundResizeListener_: null,

    get open() {
      return /** @type {!CrDialogElement} */ (this.$.dialog).open;
    },

    /**
     * Resizes the dialog to the given width/height, taking into account the
     * window width/height.
     * @param {number} width The desired height of the dialog contents.
     * @param {number} height The desired width of the dialog contents.
     * @private
     */
    updateDialogSize_: function(width, height) {
      const headerHeight = this.$.body.offsetTop;
      const maxHeight = Math.min(0.9 * window.innerHeight, MAX_HEIGHT);
      const effectiveHeight = Math.min(maxHeight, headerHeight + height);
      const effectiveWidth = Math.max(MIN_WIDTH, width);

      // Get a reference to the inner native <dialog>.
      const nativeDialog =
          /** @type {!CrDialogElement} */ (this.$.dialog).getNative();
      nativeDialog.style.height = `${effectiveHeight}px`;
      nativeDialog.style.width = `${effectiveWidth}px`;
    },

    /** @param {chrome.developerPrivate.ExtensionInfo} data */
    show: function(data) {
      this.data_ = data;
      whenDocumentReady().then(() => {
        if (!this.extensionOptions_)
          this.extensionOptions_ = document.createElement('ExtensionOptions');
        this.extensionOptions_.extension = this.data_.id;
        this.extensionOptions_.onclose = this.close.bind(this);

        let preferredSize = null;
        this.extensionOptions_.onpreferredsizechanged = e => {
          preferredSize = e;
          if (!this.$.dialog.open)
            this.$.dialog.showModal();
          // Updating the dialog size can result in a preferred size change, so
          // wait until request animation frame fires before updating the dialog
          // size. This hysteresis prevents the preferred size from oscillating
          // (see: https://crbug.com/882835).
          requestAnimationFrame(() => {
            this.updateDialogSize_(preferredSize.width, preferredSize.height);
          });
        };

        this.boundResizeListener_ = () => {
          this.updateDialogSize_(preferredSize.width, preferredSize.height);
        };

        // Add a 'resize' such that the dialog is resized when window size
        // changes.
        window.addEventListener('resize', this.boundResizeListener_);
        this.$.body.appendChild(this.extensionOptions_);
      });
    },

    close: function() {
      /** @type {!CrDialogElement} */ (this.$.dialog).close();
    },

    /** @private */
    onClose_: function() {
      this.extensionOptions_.onpreferredsizechanged = null;

      if (this.boundResizeListener_) {
        window.removeEventListener('resize', this.boundResizeListener_);
        this.boundResizeListener_ = null;
      }

      const currentPage = extensions.navigation.getCurrentPage();
      // We update the page when the options dialog closes, but only if we're
      // still on the details page. We could be on a different page if the
      // user hit back while the options dialog was visible; in that case, the
      // new page is already correct.
      if (currentPage && currentPage.page == Page.DETAILS) {
        // This will update the currentPage_ and the NavigationHelper; since
        // the active page is already the details page, no main page
        // transition occurs.
        extensions.navigation.navigateTo(
            {page: Page.DETAILS, extensionId: currentPage.extensionId});
      }
    },
  });

  return {
    OptionsDialog: OptionsDialog,
    OptionsDialogMinWidth: MIN_WIDTH,
    OptionsDialogMaxHeight: MAX_HEIGHT,
  };
});
