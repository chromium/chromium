// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './action_toolbar.js';
import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxy, ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/** @type {number} */
const PROGRESS_TIMER_MS = 3000;

/**
 * The bottom margin of each scanned image in pixels.
 * @type {number}
 */
const SCANNED_IMG_MARGIN_BOTTOM_PX = 12;

/**
 * @fileoverview
 * 'scan-preview' shows a preview of a scanned document.
 */
Polymer({
  is: 'scan-preview',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private {?ScanningBrowserProxy}*/
  browserProxy_: null,

  properties: {
    /** @type {!AppState} */
    appState: {
      type: Number,
      observer: 'onAppStateChange_',
    },

    /**
     * The object URLs of the scanned images.
     * @type {!Array<string>}
     */
    objectUrls: Array,

    /** @type {number} */
    pageNumber: {
      type: Number,
      observer: 'onPageNumberChange_',
    },

    /** @type {number} */
    progressPercent: Number,

    /** @private {boolean} */
    showScannedImages_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showHelperText_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    showScanProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCancelingProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    progressTextString_: String,

    /** @private {string} */
    previewAriaLabel_: String,

    /** @private {?number} */
    progressTimer_: {
      type: Number,
      value: null,
    },

    /** @type {boolean} */
    multiPageScanChecked: {
      type: Boolean,
      observer: 'onMultiPageScanCheckedChange_',
    },

    /** @private {number} */
    currentPageInView_: {
      type: Number,
      value: 1,
    },

    /**
     * Set to true once the first scanned image from a scan is loaded. This is
     * needed to prevent checking the dimensions of every scanned image. The
     * assumption is that all scanned images share the same dimensions.
     * @private {boolean}
     */
    scannedImagesLoaded_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showActionToolbar_: {
      type: Boolean,
      computed: 'computeShowActionToolbar_(appState, multiPageScanChecked)',
    },

    /** @private {string} */
    dialogText_: String,

    /** @private {string} */
    dialogConfirmationText_: String,

    /** @private {?Function} */
    onWindowResized_: Object,
  },

  observers: [
    'setPreviewAriaLabel_(showScannedImages_, showCancelingProgress_,' +
        ' showHelperText_)',
    'setScanProgressTimer_(showScanProgress_, progressPercent)',
  ],

  /** @override */
  created() {
    // ScanningBrowserProxy is initialized when scanning_app.js is created.
    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
    this.onWindowResized_ = () => this.setActionToolbarPosition_();
  },

  /** @override */
  ready() {
    this.style.setProperty(
        '--scanned-image-margin-bottom', SCANNED_IMG_MARGIN_BOTTOM_PX + 'px');
  },

  /** @override */
  detached() {
    if (this.multiPageScanChecked) {
      window.removeEventListener('resize', this.onWindowResized_);
    }
  },

  /** @private */
  onAppStateChange_() {
    this.showScannedImages_ = this.appState === AppState.DONE ||
        this.appState === AppState.MULTI_PAGE_NEXT_ACTION;
    this.showScanProgress_ = this.appState === AppState.SCANNING ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showCancelingProgress_ = this.appState === AppState.CANCELING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showHelperText_ = !this.showScanProgress_ &&
        !this.showCancelingProgress_ && !this.showScannedImages_;

    // If no longer showing the scanned images, reset |scannedImagesLoaded_| so
    // it can be used again for the next scan job.
    if (!this.showScannedImages_) {
      this.scannedImagesLoaded_ = false;
    }
  },

  /** @private */
  onPageNumberChange_() {
    this.progressTextString_ =
        this.i18n('scanPreviewProgressText', this.pageNumber);
  },

  /**
   * Sets the ARIA label used by the preview area based on the app state and the
   * current page showing. In the initial state, use the scan preview
   * instructions from the page as the label. When the scan completes, announce
   * the total number of pages scanned.
   * @private
   */
  setPreviewAriaLabel_() {
    if (this.showScannedImages_) {
      this.browserProxy_
          .getPluralString('scannedImagesAriaLabel', this.objectUrls.length)
          .then(
              /* @type {string} */ (pluralString) => this.previewAriaLabel_ =
                  pluralString);
      return;
    }

    if (this.showCancelingProgress_) {
      this.previewAriaLabel_ = this.i18n('cancelingScanningText');
      return;
    }

    if (this.showHelperText_) {
      this.previewAriaLabel_ = this.i18n('scanPreviewHelperText');
      return;
    }
  },

  /**
   * When receiving progress updates from an ongoing scan job, only update the
   * preview section aria label after a timer elapses to prevent successive
   * progress updates from spamming ChromeVox.
   * @private
   */
  setScanProgressTimer_() {
    // Only set the timer if scanning is still in progress.
    if (!this.showScanProgress_) {
      return;
    }

    // Always announce when a page is completed. Bypass and clear any existing
    // timer and immediately update the aria label.
    if (this.progressPercent === 100) {
      clearTimeout(this.progressTimer_);
      this.onScanProgressTimerComplete_();
      return;
    }

    // If a timer is already in progress, do not set another timer.
    if (this.progressTimer_) {
      return;
    }

    this.progressTimer_ = setTimeout(
        () => this.onScanProgressTimerComplete_(), PROGRESS_TIMER_MS);
  },

  /** @private */
  onScanProgressTimerComplete_() {
    // Only update the aria label if scanning is still in progress.
    if (!this.showScanProgress_) {
      return;
    }

    this.previewAriaLabel_ = this.i18n(
        'scanningImagesAriaLabel', this.pageNumber, this.progressPercent);
    this.progressTimer_ = null;
  },

  /**
   * While scrolling, if the current page in view would change, update it and
   * set the focus CSS variable accordingly.
   * @private
   */
  onScannedImagesScroll_() {
    if (!this.multiPageScanChecked ||
        this.appState != AppState.MULTI_PAGE_NEXT_ACTION) {
      return;
    }

    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    if (scannedImages.length === 0) {
      return;
    }

    // If the current page in view stays the same, do nothing.
    const pageInView = this.getCurrentPageInView_(scannedImages);
    if (pageInView === this.currentPageInView_) {
      return;
    }

    this.setFocusedScannedImage_(scannedImages, pageInView);
  },

  /**
   * Calculates the current page in view. Returns the page number of the
   * highest page in the viewport unless that page is scrolled halfway outside
   * the viewport, then it'll return the following page number. Assumes each
   * scanned image is the same height.
   * @param {!HTMLCollection} scannedImages
   * @return {number}
   * @private
   */
  getCurrentPageInView_(scannedImages) {
    assert(this.multiPageScanChecked);

    const imageHeight = scannedImages[0].height;
    const scrollTop = this.$$('#previewDiv').scrollTop - (imageHeight * .5);

    // This is a special case for the first page since there is no margin or
    // previous page above it.
    if (scrollTop < 0) {
      return 1;
    }

    return 2 +
        Math.floor(scrollTop / (imageHeight + SCANNED_IMG_MARGIN_BOTTOM_PX));
  },

  /**
   * Sets the CSS class for the current scanned image in view so the blue border
   * will show on the correct page when hovered.
   * @param {!HTMLCollection} scannedImages
   * @param {number} pageInView
   * @private
   */
  setFocusedScannedImage_(scannedImages, pageInView) {
    assert(this.multiPageScanChecked);

    this.removeFocusFromScannedImage_(scannedImages);

    assert(pageInView > 0 && pageInView <= scannedImages.length);
    scannedImages[pageInView - 1].classList.add('focused-scanned-image');
    this.currentPageInView_ = pageInView;
  },

  /**
   * Removes the focus CSS class from the scanned image which already has it
   * then resets |currentPageInView_|.
   * @param {!HTMLCollection} scannedImages
   * @private
   */
  removeFocusFromScannedImage_(scannedImages) {
    // This condition is only true when the user chooses to remove a page from
    // the multi-page scan session. When a page gets removed, the focus is
    // cleared and not immediately set again.
    if (this.currentPageInView_ <= 0) {
      return;
    }

    assert(
        this.currentPageInView_ > 0 &&
        this.currentPageInView_ <= scannedImages.length);
    scannedImages[this.currentPageInView_ - 1].classList.remove(
        'focused-scanned-image');

    // Set to -1 because the focus has been removed from the current page and no
    // other page has it.
    this.currentPageInView_ = -1;
  },

  /**
   * Runs when a new scanned image is loaded.
   * @param {!Event} e
   * @private
   */
  onScannedImageLoaded_(e) {
    if (!this.multiPageScanChecked) {
      return;
    }

    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    this.setFocusedScannedImage_(
        scannedImages, this.getCurrentPageInView_(scannedImages));

    // The below actions only needed for the first scanned image load.
    if (this.scannedImagesLoaded_) {
      return;
    }

    this.scannedImagesLoaded_ = true;

    // |e.model| is populated by the dom-repeat element.
    this.scrollToPage_(e.model.index);
    this.setActionToolbarPosition_();
  },

  /**
   * Set the position of the action toolbar based on the size of the scanned
   * images and the current size of the app window.
   * @private
   */
  setActionToolbarPosition_() {
    assert(this.multiPageScanChecked);

    const scannedImage = this.$$('.scanned-image');
    if (!scannedImage) {
      return;
    }

    const scannedImageRect = scannedImage.getBoundingClientRect();
    const topPosition = scannedImageRect.height * .85;
    this.style.setProperty('--action-toolbar-top', topPosition + 'px');

    const leftPosition = scannedImageRect.x + (scannedImageRect.width / 2) -
        (this.$$('action-toolbar').offsetWidth / 2);
    this.style.setProperty('--action-toolbar-left', leftPosition + 'px');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowActionToolbar_() {
    return this.multiPageScanChecked &&
        this.appState == AppState.MULTI_PAGE_NEXT_ACTION;
  },

  /**
   * Called when the "show-remove-page-dialog" event fires from the action
   * toolbar button click.
   * @param {Event} e
   * @private
   */
  onShowRemovePageDialog_(e) {
    this.showRemoveOrRescanDialog_(/* isRemovePageDialog */ true, e.detail);
  },

  /**
   * Called when the "show-rescan-page-dialog" event fires from the action
   * toolbar button click.
   * @param {Event} e
   * @private
   */
  onShowRescanPageDialog_(e) {
    this.showRemoveOrRescanDialog_(/* isRemovePageDialog */ false, e.detail);
  },

  /**
   * @param {boolean} isRemovePageDialog Determines whether to show the
   *     'Remove Page' or 'Rescan Page' dialog.
   * @param {number} pageNumber
   * @private
   */
  showRemoveOrRescanDialog_(isRemovePageDialog, pageNumber) {
    // Configure the on-click action.
    this.$$('#actionButton').addEventListener('click', () => {
      this.fireDialogAction_(
          isRemovePageDialog ? 'remove-page' : 'rescan-page', pageNumber);
    }, {once: true});

    // Configure the dialog strings for the requested mode (Remove or Rescan).
    const buttonLabelKey =
        isRemovePageDialog ? 'removePageButtonLabel' : 'rescanPageButtonLabel';
    const confirmationTextKey = isRemovePageDialog ?
        'removePageConfirmationText' :
        'rescanPageConfirmationText';
    this.browserProxy_.getPluralString(buttonLabelKey, pageNumber)
        .then(
            /* @type {string} */ (pluralString) => {
              this.dialogText_ = pluralString;
              this.dialogConfirmationText_ =
                  this.i18n(confirmationTextKey, pageNumber);

              // Once strings are loaded, open the dialog.
              this.$$('#scanPreviewDialog').showModal();
            });
  },

  /**
   * @param {string} event Either the 'remove-page' or 'rescan-page' event.
   * @param {number} pageNumber
   * @private
   */
  fireDialogAction_(event, pageNumber) {
    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    this.removeFocusFromScannedImage_(scannedImages);

    // Subtract one from |pageNumber| to get the page's index.
    assert(pageNumber > 0);
    this.fire(event, pageNumber - 1);
    this.closeDialog_();
  },

  /**  @private */
  closeDialog_() {
    this.$$('#scanPreviewDialog').close();
  },

  /**
   * Scrolls down until the page at |pageIndex| is at the top of the viewport.
   * @param {number} pageIndex
   * @private
   */
  scrollToPage_(pageIndex) {
    assert(this.multiPageScanChecked);

    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    if (scannedImages.length === 0) {
      return;
    }

    assert(pageIndex >= 0 && pageIndex < scannedImages.length);
    const imageHeight = scannedImages[0].height;

    // Use |pageIndex| to calculate the number of pages needed to scroll by to
    // get to our desired page. Ex: If we want to scroll to the page with
    // |pageIndex| = 3, we should scroll past 3 pages.
    this.$$('#previewDiv').scrollTop = imageHeight * pageIndex;
  },

  /** @private */
  onMultiPageScanCheckedChange_() {
    // Only listen for window size changes during multi-page scan sessions so
    // the position of the action toolbar can be updated.
    if (this.multiPageScanChecked) {
      window.addEventListener('resize', this.onWindowResized_);
    } else {
      window.removeEventListener('resize', this.onWindowResized_);
    }
  },
});
