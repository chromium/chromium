// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accessibility_features.mojom-lite.js';
import './action_toolbar.js';
import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getAccessibilityFeaturesInterface} from './mojo_interface_provider.js';
import {AppState, ForceHiddenElementsVisibleObserverInterface} from './scanning_app_types.js';
import {ScanningBrowserProxy, ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/** @type {number} */
const PROGRESS_TIMER_MS = 3000;

/**
 * The bottom margin of each scanned image in pixels.
 * @type {number}
 */
const SCANNED_IMG_MARGIN_BOTTOM_PX = 12;

/**
 * The bottom margin for the action toolbar from the bottom edge of the
 * viewport.
 * @type {number}
 */
const ACTION_TOOLBAR_BOTTOM_MARGIN_PX = 40;

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

  /** @private {?Function} */
  onWindowResized_: null,

  /** @private {?ResizeObserver} */
  previewAreaResizeObserver_: null,

  /** @private {?Function} */
  onDialogActionClick_: null,

  /** @private {number} */
  actionToolbarHeight_: 0,

  /** @private {number} */
  actionToolbarWidth_: 0,

  /**
   * Receives the status of the enabled accesbility features that should force
   * the hidden elements visible.
   * @private {?ash.common.mojom.ForceHiddenElementsVisibleObserverReceiver}
   */
  forceHiddenElementsVisibleObserverReceiver_: null,

  properties: {
    /** @type {!AppState} */
    appState: {
      type: Number,
      observer: 'onAppStateChange_',
    },

    /** @protected {boolean} */
    isDarkModeEnabled_: {
      type: Boolean,
    },

    /** @protected {boolean} */
    isJellyEnabled_: {
      type: Boolean,
      value: () => {
        return loadTimeData.getBoolean('isJellyEnabledForScanningApp');
      },
    },

    /**
     * The object URLs of the scanned images.
     * @type {!Array<string>}
     */
    objectUrls: {
      type: Array,
      observer: 'onObjectUrlsChange_',
    },


    /** @type {number} */
    pageNumber: {
      type: Number,
      observer: 'onPageNumberChange_',
    },

    /** @type {number} */
    progressPercent: Number,

    /** @private {boolean} */
    showHelpOrProgress_: {
      type: Boolean,
      value: true,
    },

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
    isMultiPageScan: {
      type: Boolean,
      observer: 'onIsMultiPageScanChange_',
    },

    /**
     * The index of the page currently focused on.
     * @private {number}
     */
    currentPageIndexInView_: {
      type: Number,
      value: 0,
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
    showActionToolbar_: Boolean,

    /** @private {string} */
    dialogTitleText_: String,

    /** @private {string} */
    dialogConfirmationText_: String,

    /** @private {string} */
    dialogButtonText_: String,

    /**
     * True when |appState| is MULTI_PAGE_SCANNING.
     * @private {boolean}
     */
    multiPageScanning_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private {boolean} */
    showSingleImageFocus_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * True when the ChromeVox, Switch, or Screen Magnifier accessibility
     * features are turned on that require the action toolbar to always be
     * visible during multi-page scan sessions. Only used for CSS selector
     * logic.
     * @private {boolean}
     */
    forceActionToolbarVisible_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  observers: [
    'setPreviewAriaLabel_(showScannedImages_, showCancelingProgress_,' +
        ' showHelperText_, objectUrls.length)',
    'setScanProgressTimer_(showScanProgress_, progressPercent)',
  ],

  /** @override */
  created() {
    // ScanningBrowserProxy is initialized when scanning_app.js is created.
    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
    this.onWindowResized_ = () => this.setActionToolbarPosition_();
    this.previewAreaResizeObserver_ =
        new ResizeObserver(() => this.updatePreviewElements_());
  },

  /** @override */
  ready() {
    this.style.setProperty(
        '--scanned-image-margin-bottom', SCANNED_IMG_MARGIN_BOTTOM_PX + 'px');

    // parseFloat() is used to convert the string returned by
    // styleMap.get() into a number ("642px" --> 642).
    const styleMap = this.computedStyleMap();
    this.actionToolbarHeight_ =
        parseFloat(styleMap.get('--action-toolbar-height').toString());
    this.actionToolbarWidth_ =
        parseFloat(styleMap.get('--action-toolbar-width').toString());

    this.forceHiddenElementsVisibleObserverReceiver_ =
        new ash.common.mojom.ForceHiddenElementsVisibleObserverReceiver(
            /**
               @type {!ForceHiddenElementsVisibleObserverInterface}
             */
            (this));
    getAccessibilityFeaturesInterface()
        .observeForceHiddenElementsVisible(
            this.forceHiddenElementsVisibleObserverReceiver_.$
                .bindNewPipeAndPassRemote())
        .then(
            response => this.forceActionToolbarVisible_ =
                response.forceVisible);
  },

  /** @override */
  detached() {
    if (this.isMultiPageScan) {
      window.removeEventListener('resize', this.onWindowResized_);
      this.previewAreaResizeObserver_.disconnect();
    }

    if (this.forceHiddenElementsVisibleObserverReceiver_) {
      this.forceHiddenElementsVisibleObserverReceiver_.$.close();
    }
  },

  /**
   * Overrides ash.common.mojom.ForceHiddenElementsVisibleObserverReceiver.
   * @param {boolean} forceVisible
   */
  onForceHiddenElementsVisibleChange(forceVisible) {
    this.forceActionToolbarVisible_ = forceVisible;
  },

  /** @private */
  onAppStateChange_() {
    this.showScannedImages_ = this.appState === AppState.DONE ||
        this.appState === AppState.MULTI_PAGE_NEXT_ACTION ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showScanProgress_ = this.appState === AppState.SCANNING ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showCancelingProgress_ = this.appState === AppState.CANCELING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showHelperText_ = !this.showScanProgress_ &&
        !this.showCancelingProgress_ && !this.showScannedImages_;
    this.showHelpOrProgress_ = !this.showScannedImages_ ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.multiPageScanning_ = this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showSingleImageFocus_ =
        this.appState === AppState.MULTI_PAGE_NEXT_ACTION;
    this.showActionToolbar_ = this.appState === AppState.MULTI_PAGE_NEXT_ACTION;

    // If no longer showing the scanned images, reset |scannedImagesLoaded_| so
    // it can be used again for the next scan job.
    if (this.showHelpOrProgress_) {
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
    if (!this.isMultiPageScan ||
        this.appState != AppState.MULTI_PAGE_NEXT_ACTION) {
      return;
    }

    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    if (scannedImages.length === 0) {
      return;
    }

    // If the current page in view stays the same, do nothing.
    const pageIndexInView = this.getCurrentPageInView_(scannedImages);
    if (pageIndexInView === this.currentPageIndexInView_) {
      return;
    }

    this.setFocusedScannedImage_(scannedImages, pageIndexInView);
  },

  /**
   * Calculates the index of the current page in view based on the scroll
   * position. This algorithm allows for every scanned image to be focusable
   * via scrolling. It starts by waiting until the previous image is scrolled
   * halfway outside the viewport before the page index changes, but then
   * changes behavior once the end of the scroll area is reached and no more
   * images can be scrolled up. In that case, the remaining scroll area is
   * divided evenly between the final images in the viewport.
   * @param {!HTMLCollection} scannedImages
   * @return {number}
   * @private
   */
  getCurrentPageInView_(scannedImages) {
    assert(this.isMultiPageScan);

    if (scannedImages.length === 1) {
      return 0;
    }

    // Assumes the scanned images share the same dimensions.
    const imageHeight = scannedImages[0].getBoundingClientRect().height +
        SCANNED_IMG_MARGIN_BOTTOM_PX;

    // The first step is to calculate the number of images that will be visible
    // in the viewport when scrolled to the bottom. That is how to calculate the
    // "crossover" point where the algorithm needs to change.
    const numImagesVisibleAtEnd =
        Math.ceil(this.$$('#previewDiv').offsetHeight / imageHeight);
    const numImagesBeforeCrossover =
        scannedImages.length - numImagesVisibleAtEnd;

    // Calculate the point where the last images in the scroll area are visible
    // and the scrolling algorithm needs to change.
    const crossoverBreakpoint = numImagesBeforeCrossover == 0 ?
        Number.MIN_VALUE :
        scannedImages[numImagesBeforeCrossover].offsetTop - (imageHeight / 2);

    // Before the "crossover", update the page index based on when the previous
    // image is scrolled halfway outside the viewport.
    if (this.$$('#previewDiv').scrollTop < crossoverBreakpoint) {
      // Subtract half the image height so |scrollTop| = 0 when the first page
      // is scrolled halfway outside the viewport. That way each page index will
      // be the current scroll divided by the image height.
      const scrollTop = this.$$('#previewDiv').scrollTop - (imageHeight / 2) -
          /*imageFocusBorder=*/ 2;
      if (scrollTop < 0) {
        return 0;
      }

      return 1 + Math.floor(scrollTop / imageHeight);
    }

    // After the "crossover", the remaining amount of scroll left in the
    // scrollbar is divided evenly to the remaining images. This allows every
    // image to be scrolled to.
    const maxScrollTop = this.$$('#previewDiv').scrollHeight -
        this.$$('#previewDiv').offsetHeight;
    const scrollRemainingAfterCrossover =
        Math.max(maxScrollTop - crossoverBreakpoint, 0);
    const imageScrollProportion =
        scrollRemainingAfterCrossover / numImagesVisibleAtEnd;

    // Calculate the new page index.
    const scrollTop = this.$$('#previewDiv').scrollTop - crossoverBreakpoint;
    const index = Math.floor(scrollTop / imageScrollProportion);

    return Math.min(numImagesBeforeCrossover + index, scannedImages.length - 1);
  },

  /**
   * Sets the CSS class for the current scanned image in view so the blue border
   * will show on the correct page when hovered.
   * @param {!HTMLCollection} scannedImages
   * @param {number} pageIndexInView
   * @private
   */
  setFocusedScannedImage_(scannedImages, pageIndexInView) {
    assert(this.isMultiPageScan);

    this.removeFocusFromScannedImage_(scannedImages);

    assert(pageIndexInView >= 0 && pageIndexInView < scannedImages.length);
    scannedImages[pageIndexInView].classList.add('focused-scanned-image');
    this.currentPageIndexInView_ = pageIndexInView;
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
    if (this.currentPageIndexInView_ < 0) {
      return;
    }

    assert(
        this.currentPageIndexInView_ >= 0 &&
        this.currentPageIndexInView_ < scannedImages.length);
    scannedImages[this.currentPageIndexInView_].classList.remove(
        'focused-scanned-image');

    // Set to -1 because the focus has been removed from the current page and no
    // other page has it.
    this.currentPageIndexInView_ = -1;
  },

  /**
   * Runs when a new scanned image is loaded.
   * @param {!Event} e
   * @private
   */
  onScannedImageLoaded_(e) {
    if (!this.isMultiPageScan) {
      return;
    }

    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    this.setFocusedScannedImage_(
        scannedImages, this.getCurrentPageInView_(scannedImages));

    this.updatePreviewElements_();

    // Scrolling to a page is only needed for the first scanned image load.
    if (this.scannedImagesLoaded_) {
      return;
    }

    this.scannedImagesLoaded_ = true;

    // |e.model| is populated by the dom-repeat element.
    this.scrollToPage_(e.model.index);
  },

  /**
   * Set the focus to the clicked scanned image.
   * @param {!Event} e
   * @private
   */
  onScannedImageClick_(e) {
    if (!this.isMultiPageScan) {
      return;
    }

    // |e.model| is populated by the dom-repeat element.
    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    this.setFocusedScannedImage_(scannedImages, e.model.index);
  },

  /**
   * Set the position of the action toolbar based on the size of the scanned
   * images and the current size of the app window.
   * @private
   */
  setActionToolbarPosition_() {
    assert(this.isMultiPageScan);

    const scannedImage = this.$$('.scanned-image');
    if (!scannedImage) {
      return;
    }

    const scannedImageRect = scannedImage.getBoundingClientRect();

    // Set the toolbar position from the bottom edge of the viewport.
    const topPosition = this.$$('#previewDiv').offsetHeight -
        ACTION_TOOLBAR_BOTTOM_MARGIN_PX - (this.actionToolbarHeight_ / 2);
    this.style.setProperty('--action-toolbar-top', topPosition + 'px');

    // Position the toolbar in the middle of the viewport.
    const leftPosition = scannedImageRect.x + (scannedImageRect.width / 2) -
        (this.actionToolbarWidth_ / 2);
    this.style.setProperty('--action-toolbar-left', leftPosition + 'px');
  },

  /** @param {boolean} enabled */
  setIsJellyEnabledForTesting(enabled) {
    this.isJellyEnabled_ = enabled;
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
   * @param {number} pageIndex
   * @private
   */
  showRemoveOrRescanDialog_(isRemovePageDialog, pageIndex) {
    // Configure the on-click action.
    this.onDialogActionClick_ = () => {
      this.fireDialogAction_(
          isRemovePageDialog ? 'remove-page' : 'rescan-page', pageIndex);
    };
    this.$$('#actionButton')
        .addEventListener('click', this.onDialogActionClick_, {once: true});

    // Configure the dialog strings for the requested mode (Remove or Rescan).
    this.dialogButtonText_ = this.i18n(
        isRemovePageDialog ? 'removePageButtonLabel' : 'rescanPageButtonLabel');

    this.dialogConfirmationText_ = this.i18n(
        isRemovePageDialog ? 'removePageConfirmationText' :
                             'rescanPageConfirmationText');
    this.browserProxy_
        .getPluralString(
            isRemovePageDialog ? 'removePageDialogTitle' :
                                 'rescanPageDialogTitle',
            this.objectUrls.length === 1 ? 0 : pageIndex + 1)
        .then(
            /* @type {string} */ (pluralString) => {
              // When removing a page while more than one page exists, leave the
              // title empty and move the title text into the body.
              const isRemoveFromMultiplePages =
                  isRemovePageDialog && this.objectUrls.length > 1;
              this.dialogTitleText_ =
                  isRemoveFromMultiplePages ? '' : pluralString;
              if (isRemoveFromMultiplePages) {
                this.dialogConfirmationText_ = pluralString;
              }

              // Once strings are loaded, open the dialog.
              this.$$('#scanPreviewDialog').showModal();
            });
  },

  /**
   * @param {string} event Either the 'remove-page' or 'rescan-page' event.
   * @param {number} pageIndex
   * @private
   */
  fireDialogAction_(event, pageIndex) {
    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    this.removeFocusFromScannedImage_(scannedImages);

    assert(pageIndex >= 0);
    this.fire(event, pageIndex);
    this.closeDialog_();
  },

  /**  @private */
  closeDialog_() {
    this.$$('#scanPreviewDialog').close();
    this.$$('#actionButton')
        .removeEventListener('click', this.onDialogActionClick_);
  },

  /**
   * Scrolls the image specified by |pageIndex| into view.
   * @param {number} pageIndex
   * @private
   */
  scrollToPage_(pageIndex) {
    assert(this.isMultiPageScan);

    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    if (scannedImages.length === 0) {
      return;
    }

    assert(pageIndex >= 0 && pageIndex < scannedImages.length);
    this.$$('#previewDiv').scrollTop =
        scannedImages[pageIndex].offsetTop - /*imageFocusBorder=*/ 2;
  },

  /** @private */
  onIsMultiPageScanChange_() {
    // Listen for window size changes during multi-page scan sessions so the
    // position of the action toolbar can be updated.
    if (this.isMultiPageScan) {
      window.addEventListener('resize', this.onWindowResized_);

      // Observe changes to the preview area during multi-page scan sessions so
      // the scan progress div height can be updated when images are
      // added/removed.
      this.previewAreaResizeObserver_.observe(
          /** @type {!HTMLElement} */ (this.$$('#previewDiv')));
    } else {
      window.removeEventListener('resize', this.onWindowResized_);
      this.previewAreaResizeObserver_.disconnect();
    }
  },

  /**
   * Make the scan progress height match the preview area height.
   * @private
   */
  setMultiPageScanProgressHeight_() {
    this.style.setProperty(
        '--multi-page-scan-progress-height',
        this.$$('#previewDiv').offsetHeight + 'px');
  },

  /** @private */
  onObjectUrlsChange_() {
    if (!this.isMultiPageScan) {
      return;
    }

    // Set to -1 when no pages exist after a scan is saved.
    if (this.objectUrls.length === 0) {
      this.currentPageIndexInView_ = -1;
    }
  },

  /**
   * Sets the size and positioning of elements that depend on the size of the
   * scan preview area.
   * @private
   */
  updatePreviewElements_() {
    this.setMultiPageScanProgressHeight_();
    this.setActionToolbarPosition_();
  },

  /**
   * Hide the action toolbar if it's page is not currently in view.
   * @return {boolean}
   * @private
   */
  showActionToolbarByIndex_(index) {
    return index === this.currentPageIndexInView_ && this.showActionToolbar_;
  },

  /**
   * Set |currentPageIndexInView_| to the page focused on (via ChromeVox).
   * @param {!Event} e
   * @private
   */
  onScannedImageInFocus_(e) {
    if (!this.isMultiPageScan) {
      return;
    }

    // |e.model| is populated by the dom-repeat element.
    const scannedImages =
        this.$$('#scannedImages').getElementsByClassName('scanned-image');
    this.setFocusedScannedImage_(scannedImages, e.model.index);
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getScannedImageAriaLabel_(index) {
    return this.i18n(
        'multiPageImageAriaLabel', index + 1, this.objectUrls.length);
  },

  /**
   * Determines correct SVG for "ready to scan" based on dark mode.
   * @protected
   * @return {string}
   */
  getReadyToScanSvgSrc_() {
    return this.isDarkModeEnabled_ ? 'svg/ready_to_scan_dark.svg' :
                                     'svg/ready_to_scan.svg';
  },
});
