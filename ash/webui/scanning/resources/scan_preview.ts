// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './action_toolbar.js';
import './scanning_fonts.css.js';
import './scanning_shared.css.js';
import './strings.m.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ForceHiddenElementsVisibleObserverInterface, ForceHiddenElementsVisibleObserverReceiver} from './accessibility_features.mojom-webui.js';
import {getAccessibilityFeaturesInterface} from './mojo_interface_provider.js';
import {getTemplate} from './scan_preview.html.js';
import {AppState} from './scanning_app_types.js';
import {ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

const PROGRESS_TIMER_MS = 3000;

/**
 * The bottom margin of each scanned image in pixels.
 */
const SCANNED_IMG_MARGIN_BOTTOM_PX = 12;

/**
 * The bottom margin for the action toolbar from the bottom edge of the
 * viewport.
 */
const ACTION_TOOLBAR_BOTTOM_MARGIN_PX = 40;

/**
 * @fileoverview
 * 'scan-preview' shows a preview of a scanned document.
 */

const ScanPreviewElementBase = I18nMixin(PolymerElement);

type DialogAction = 'rescan-page'|'remove-page';

export class ScanPreviewElement extends ScanPreviewElementBase implements
    ForceHiddenElementsVisibleObserverInterface {
  static get is() {
    return 'scan-preview' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      appState: {
        type: Number,
        observer: ScanPreviewElement.prototype.appStateChanged,
      },

      /**
       * The object URLs of the scanned images.
       */
      objectUrls: {
        type: Array,
        observer: ScanPreviewElement.prototype.objectUrlsChanged,
      },

      pageNumber: {
        type: Number,
        observer: ScanPreviewElement.prototype.pageNumberChanged,
      },

      progressPercent: Number,

      showHelpOrProgress: {
        type: Boolean,
        value: true,
      },

      showScannedImages: {
        type: Boolean,
        value: false,
      },

      showHelperText: {
        type: Boolean,
        value: true,
      },

      showScanProgress: {
        type: Boolean,
        value: false,
      },

      showCancelingProgress: {
        type: Boolean,
        value: false,
      },

      progressTextString: String,

      previewAriaLabel: String,

      progressTimer: {
        type: Number,
        value: null,
      },

      isMultiPageScan: {
        type: Boolean,
        observer: ScanPreviewElement.prototype.isMultiPageScanChanged,
      },

      /**
       * The index of the page currently focused on.
       */
      currentPageIndexInView: {
        type: Number,
        value: 0,
      },

      /**
       * Set to true once the first scanned image from a scan is loaded. This is
       * needed to prevent checking the dimensions of every scanned image. The
       * assumption is that all scanned images share the same dimensions.
       */
      scannedImagesLoaded: {
        type: Boolean,
        value: false,
      },

      showActionToolbar: Boolean,

      dialogTitleText: String,

      dialogConfirmationText: String,

      dialogButtonText: String,

      /**
       * True when |appState| is MULTI_PAGE_SCANNING.
       */
      multiPageScanning: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showSingleImageFocus: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * True when the ChromeVox, Switch, or Screen Magnifier accessibility
       * features are turned on that require the action toolbar to always be
       * visible during multi-page scan sessions. Only used for CSS selector
       * logic.
       */
      forceActionToolbarVisible: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return [
      'setPreviewAriaLabel(showScannedImages, showCancelingProgress,' +
          ' showHelperText, objectUrls.length)',
      'setScanProgressTimer(showScanProgress, progressPercent)',

    ];
  }

  appState: AppState;
  objectUrls: string[];
  private pageNumber: number;
  private progressPercent: number;
  private showHelpOrProgress: boolean;
  private showScannedImages: boolean;
  private showHelperText: boolean;
  private showScanProgress: boolean;
  private showCancelingProgress: boolean;
  private progressTextString: string;
  private previewAriaLabel: string;
  private progressTimer: number|null;
  private isMultiPageScan: boolean;
  private currentPageIndexInView: number;
  private scannedImagesLoaded: boolean;
  private showActionToolbar: boolean;
  private dialogTitleText: string;
  private dialogConfirmationText: string;
  private dialogButtonText: string;
  private multiPageScanning: boolean;
  private showSingleImageFocus: boolean;
  private forceActionToolbarVisible: boolean;
  private actionToolbarHeight: number;
  private actionToolbarWidth: number;
  private forceHiddenElementsVisibleObserverReceiver:
      ForceHiddenElementsVisibleObserverReceiver;
  private onDialogActionClick: EventListenerOrEventListenerObject;
  private onWindowResized: EventListenerOrEventListenerObject;
  private previewAreaResizeObserver: ResizeObserver;
  // ScanningBrowserProxy is initialized when scanning_app.js is created.
  private browserProxy = ScanningBrowserProxyImpl.getInstance();

  constructor() {
    super();

    this.onWindowResized = () => this.setActionToolbarPosition();
    this.previewAreaResizeObserver =
        new ResizeObserver(() => this.updatePreviewElements());
  }

  override ready(): void {
    super.ready();

    this.style.setProperty(
        '--scanned-image-margin-bottom', SCANNED_IMG_MARGIN_BOTTOM_PX + 'px');

    // parseFloat() is used to convert the string returned by
    // styleMap.get() into a number ("642px" --> 642).
    const styleMap = (this as unknown as Element).computedStyleMap();
    this.actionToolbarHeight =
        parseFloat(styleMap.get('--action-toolbar-height')!.toString());
    this.actionToolbarWidth =
        parseFloat(styleMap.get('--action-toolbar-width')!.toString());

    this.forceHiddenElementsVisibleObserverReceiver =
        new ForceHiddenElementsVisibleObserverReceiver(this);
    getAccessibilityFeaturesInterface()
        .observeForceHiddenElementsVisible(
            this.forceHiddenElementsVisibleObserverReceiver.$
                .bindNewPipeAndPassRemote())
        .then(
            response => this.forceActionToolbarVisible = response.forceVisible);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.isMultiPageScan) {
      window.removeEventListener('resize', this.onWindowResized);
      this.previewAreaResizeObserver.disconnect();
    }

    if (this.forceHiddenElementsVisibleObserverReceiver) {
      this.forceHiddenElementsVisibleObserverReceiver.$.close();
    }
  }

  /**
   * Overrides ForceHiddenElementsVisibleObserverReceiver.
   */
  onForceHiddenElementsVisibleChange(forceVisible: boolean): void {
    this.forceActionToolbarVisible = forceVisible;
  }

  private appStateChanged(): void {
    this.showScannedImages = this.appState === AppState.DONE ||
        this.appState === AppState.MULTI_PAGE_NEXT_ACTION ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showScanProgress = this.appState === AppState.SCANNING ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showCancelingProgress = this.appState === AppState.CANCELING ||
        this.appState === AppState.MULTI_PAGE_CANCELING;
    this.showHelperText = !this.showScanProgress &&
        !this.showCancelingProgress && !this.showScannedImages;
    this.showHelpOrProgress = !this.showScannedImages ||
        this.appState === AppState.MULTI_PAGE_SCANNING;
    this.multiPageScanning = this.appState === AppState.MULTI_PAGE_SCANNING;
    this.showSingleImageFocus =
        this.appState === AppState.MULTI_PAGE_NEXT_ACTION;
    this.showActionToolbar = this.appState === AppState.MULTI_PAGE_NEXT_ACTION;

    // If no longer showing the scanned images, reset |scannedImagesLoaded_| so
    // it can be used again for the next scan job.
    if (this.showHelpOrProgress) {
      this.scannedImagesLoaded = false;
    }
  }

  private pageNumberChanged(): void {
    this.progressTextString =
        this.i18n('scanPreviewProgressText', this.pageNumber);
  }

  /**
   * Sets the ARIA label used by the preview area based on the app state and the
   * current page showing. In the initial state, use the scan preview
   * instructions from the page as the label. When the scan completes, announce
   * the total number of pages scanned.
   *
   */
  private setPreviewAriaLabel(): void {
    if (this.showScannedImages) {
      this.browserProxy
          .getPluralString('scannedImagesAriaLabel', this.objectUrls.length)
          .then((pluralString) => this.previewAriaLabel = pluralString);
      return;
    }

    if (this.showCancelingProgress) {
      this.previewAriaLabel = this.i18n('cancelingScanningText');
      return;
    }

    if (this.showHelperText) {
      this.previewAriaLabel = this.i18n('scanPreviewHelperText');
      return;
    }
  }

  /**
   * When receiving progress updates from an ongoing scan job, only update the
   * preview section aria label after a timer elapses to prevent successive
   * progress updates from spamming ChromeVox.
   */
  private setScanProgressTimer(): void {
    // Only set the timer if scanning is still in progress.
    if (!this.showScanProgress) {
      return;
    }

    // Always announce when a page is completed. Bypass and clear any existing
    // timer and immediately update the aria label.
    if (this.progressPercent === 100) {
      if (this.progressTimer) {
        clearTimeout(this.progressTimer);
      }
      this.onScanProgressTimerComplete();
      return;
    }

    // If a timer is already in progress, do not set another timer.
    if (this.progressTimer) {
      return;
    }

    this.progressTimer =
        setTimeout(() => this.onScanProgressTimerComplete(), PROGRESS_TIMER_MS);
  }

  private onScanProgressTimerComplete(): void {
    // Only update the aria label if scanning is still in progress.
    if (!this.showScanProgress) {
      return;
    }

    this.previewAriaLabel = this.i18n(
        'scanningImagesAriaLabel', this.pageNumber, this.progressPercent);
    this.progressTimer = null;
  }

  /**
   * While scrolling, if the current page in view would change, update it and
   * set the focus CSS variable accordingly.
   */
  private onScannedImagesScroll(): void {
    if (!this.isMultiPageScan ||
        this.appState != AppState.MULTI_PAGE_NEXT_ACTION) {
      return;
    }

    const scannedImagesDiv: HTMLDivElement =
        this.shadowRoot!.querySelector<HTMLDivElement>('#scannedImages')!;
    const scannedImages: HTMLCollection =
        scannedImagesDiv.getElementsByClassName('scanned-image');
    if (scannedImages.length === 0) {
      return;
    }

    // If the current page in view stays the same, do nothing.
    const pageIndexInView = this.getCurrentPageInView(scannedImages);
    if (pageIndexInView === this.currentPageIndexInView) {
      return;
    }

    this.setFocusedScannedImage(scannedImages, pageIndexInView);
  }

  /**
   * Calculates the index of the current page in view based on the scroll
   * position. This algorithm allows for every scanned image to be focusable
   * via scrolling. It starts by waiting until the previous image is scrolled
   * halfway outside the viewport before the page index changes, but then
   * changes behavior once the end of the scroll area is reached and no more
   * images can be scrolled up. In that case, the remaining scroll area is
   * divided evenly between the final images in the viewport.
   */
  private getCurrentPageInView(scannedImages: HTMLCollection): number {
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
    const numImagesVisibleAtEnd = Math.ceil(
        this.shadowRoot!.querySelector<HTMLElement>(
                            '#previewDiv')!.offsetHeight /
        imageHeight);
    const numImagesBeforeCrossover =
        scannedImages.length - numImagesVisibleAtEnd;

    // Calculate the point where the last images in the scroll area are visible
    // and the scrolling algorithm needs to change.
    const crossoverBreakpoint = numImagesBeforeCrossover == 0 ?
        Number.MIN_VALUE :
        (scannedImages[numImagesBeforeCrossover] as HTMLElement).offsetTop -
            (imageHeight / 2);

    // Before the "crossover", update the page index based on when the previous
    // image is scrolled halfway outside the viewport.
    if (this.shadowRoot!.querySelector<HTMLElement>('#previewDiv')!.scrollTop <
        crossoverBreakpoint) {
      // Subtract half the image height so |scrollTop| = 0 when the first page
      // is scrolled halfway outside the viewport. That way each page index will
      // be the current scroll divided by the image height.
      const scrollTop =
          this.shadowRoot!.querySelector<HTMLElement>(
                              '#previewDiv')!.scrollTop -
          (imageHeight / 2) -
          /*imageFocusBorder=*/ 2;
      if (scrollTop < 0) {
        return 0;
      }

      return 1 + Math.floor(scrollTop / imageHeight);
    }

    // After the "crossover", the remaining amount of scroll left in the
    // scrollbar is divided evenly to the remaining images. This allows every
    // image to be scrolled to.
    const maxScrollTop =
        this.shadowRoot!.querySelector<HTMLElement>(
                            '#previewDiv')!.scrollHeight -
        this.shadowRoot!.querySelector<HTMLElement>(
                            '#previewDiv')!.offsetHeight;
    const scrollRemainingAfterCrossover =
        Math.max(maxScrollTop - crossoverBreakpoint, 0);
    const imageScrollProportion =
        scrollRemainingAfterCrossover / numImagesVisibleAtEnd;

    // Calculate the new page index.
    const scrollTop =
        this.shadowRoot!.querySelector<HTMLElement>('#previewDiv')!.scrollTop -
        crossoverBreakpoint;
    const index = Math.floor(scrollTop / imageScrollProportion);

    return Math.min(numImagesBeforeCrossover + index, scannedImages.length - 1);
  }

  /**
   * Sets the CSS class for the current scanned image in view so the blue border
   * will show on the correct page when hovered.
   */
  private setFocusedScannedImage(
      scannedImages: HTMLCollection, pageIndexInView: number): void {
    assert(this.isMultiPageScan);

    this.removeFocusFromScannedImage(scannedImages);

    assert(pageIndexInView >= 0 && pageIndexInView < scannedImages.length);
    scannedImages[pageIndexInView].classList.add('focused-scanned-image');
    this.currentPageIndexInView = pageIndexInView;
  }

  /**
   * Removes the focus CSS class from the scanned image which already has it
   * then resets |currentPageInView_|.
   */
  private removeFocusFromScannedImage(scannedImages: HTMLCollection): void {
    // This condition is only true when the user chooses to remove a page from
    // the multi-page scan session. When a page gets removed, the focus is
    // cleared and not immediately set again.
    if (this.currentPageIndexInView < 0) {
      return;
    }

    assert(
        this.currentPageIndexInView >= 0 &&
        this.currentPageIndexInView < scannedImages.length);
    scannedImages[this.currentPageIndexInView].classList.remove(
        'focused-scanned-image');

    // Set to -1 because the focus has been removed from the current page and no
    // other page has it.
    this.currentPageIndexInView = -1;
  }

  /**
   * Runs when a new scanned image is loaded.
   */
  private onScannedImageLoaded(e: DomRepeatEvent<string>): void {
    if (!this.isMultiPageScan) {
      return;
    }

    const scannedImages =
        this.shadowRoot!.querySelector<HTMLElement>('#scannedImages')!
            .getElementsByClassName('scanned-image');
    this.setFocusedScannedImage(
        scannedImages, this.getCurrentPageInView(scannedImages));

    this.updatePreviewElements();

    // Scrolling to a page is only needed for the first scanned image load.
    if (this.scannedImagesLoaded) {
      return;
    }

    this.scannedImagesLoaded = true;

    // |e.model| is populated by the dom-repeat element.
    this.scrollToPage(e.model.index);
  }

  /**
   * Set the focus to the clicked scanned image.
   */
  private onScannedImageClick(e: DomRepeatEvent<string>): void {
    if (!this.isMultiPageScan) {
      return;
    }

    // |e.model| is populated by the dom-repeat element.
    const scannedImages =
        this.shadowRoot!.querySelector<HTMLDivElement>('#scannedImages')!
            .getElementsByClassName('scanned-image');
    this.setFocusedScannedImage(scannedImages, e.model.index);
  }

  /**
   * Set the position of the action toolbar based on the size of the scanned
   * images and the current size of the app window.
   */
  private setActionToolbarPosition(): void {
    assert(this.isMultiPageScan);

    const scannedImage =
        this.shadowRoot!.querySelector<HTMLImageElement>('.scanned-image')!;
    if (!scannedImage) {
      return;
    }

    const scannedImageRect = scannedImage.getBoundingClientRect();

    // Set the toolbar position from the bottom edge of the viewport.
    const topPosition =
        this.shadowRoot!.querySelector<HTMLDivElement>(
                            '#previewDiv')!.offsetHeight -
        ACTION_TOOLBAR_BOTTOM_MARGIN_PX - (this.actionToolbarHeight / 2);
    this.style.setProperty('--action-toolbar-top', topPosition + 'px');

    // Position the toolbar in the middle of the viewport.
    const leftPosition = scannedImageRect.x + (scannedImageRect.width / 2) -
        (this.actionToolbarWidth / 2);
    this.style.setProperty('--action-toolbar-left', leftPosition + 'px');
  }

  /**
   * Called when the "show-remove-page-dialog" event fires from the action
   * toolbar button click.
   */
  private onShowRemovePageDialog(e: CustomEvent<number>): void {
    this.showRemoveOrRescanDialog(/* isRemovePageDialog */ true, e.detail);
  }

  /**
   * Called when the "show-rescan-page-dialog" event fires from the action
   * toolbar button click.
   */
  private onShowRescanPageDialog(e: CustomEvent<number>): void {
    this.showRemoveOrRescanDialog(/* isRemovePageDialog */ false, e.detail);
  }

  /**
   * |isRemovePageDialog| determines whether to show the 'Remove Page' or
   * 'Rescan Page' dialog.
   */
  private showRemoveOrRescanDialog(
      isRemovePageDialog: boolean, pageIndex: number): void {
    // Configure the on-click action.
    this.onDialogActionClick = () => {
      this.fireDialogAction(
          isRemovePageDialog ? 'remove-page' : 'rescan-page', pageIndex);
    };
    this.shadowRoot!.querySelector<CrButtonElement>('#actionButton')!
        .addEventListener('click', this.onDialogActionClick, {once: true});

    // Configure the dialog strings for the requested mode (Remove or Rescan).
    this.dialogButtonText = this.i18n(
        isRemovePageDialog ? 'removePageButtonLabel' : 'rescanPageButtonLabel');

    this.dialogConfirmationText = this.i18n(
        isRemovePageDialog ? 'removePageConfirmationText' :
                             'rescanPageConfirmationText');
    this.browserProxy
        .getPluralString(
            isRemovePageDialog ? 'removePageDialogTitle' :
                                 'rescanPageDialogTitle',
            this.objectUrls.length === 1 ? 0 : pageIndex + 1)
        .then((pluralString: string): void => {
          // When removing a page while more than one page exists, leave the
          // title empty and move the title text into the body.
          const isRemoveFromMultiplePages =
              isRemovePageDialog && this.objectUrls.length > 1;
          this.dialogTitleText = isRemoveFromMultiplePages ? '' : pluralString;
          if (isRemoveFromMultiplePages) {
            this.dialogConfirmationText = pluralString;
          }

          // Once strings are loaded, open the dialog.
          this.shadowRoot!.querySelector<CrDialogElement>(
                              '#scanPreviewDialog')!.showModal();
        });
  }

  /**
   * Filrs either the 'remove-page' or 'rescan-page' event.
   */
  private fireDialogAction(event: DialogAction, pageIndex: number): void {
    const scannedImages =
        this.shadowRoot!.querySelector<HTMLDivElement>('#scannedImages')!
            .getElementsByClassName('scanned-image');
    this.removeFocusFromScannedImage(scannedImages);

    assert(pageIndex >= 0);
    this.dispatchEvent(new CustomEvent(
        event, {bubbles: true, composed: true, detail: pageIndex}));
    this.closeDialog();
  }

  private closeDialog(): void {
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#scanPreviewDialog')!.close();
    this.shadowRoot!.querySelector<CrButtonElement>('#actionButton')!
        .removeEventListener('click', this.onDialogActionClick);
  }

  /**
   * Scrolls the image specified by |pageIndex| into view.
   */
  private scrollToPage(pageIndex: number): void {
    assert(this.isMultiPageScan);

    const scannedImages =
        this.shadowRoot!.querySelector<HTMLDivElement>('#scannedImages')!
            .getElementsByClassName('scanned-image');
    if (scannedImages.length === 0) {
      return;
    }

    assert(pageIndex >= 0 && pageIndex < scannedImages.length);
    this.shadowRoot!.querySelector<HTMLElement>('#previewDiv')!.scrollTop =
        (scannedImages[pageIndex] as HTMLElement).offsetTop -
        /*imageFocusBorder=*/ 2;
  }

  private isMultiPageScanChanged(): void {
    // Listen for window size changes during multi-page scan sessions so the
    // position of the action toolbar can be updated.
    if (this.isMultiPageScan) {
      window.addEventListener('resize', this.onWindowResized);

      // Observe changes to the preview area during multi-page scan sessions so
      // the scan progress div height can be updated when images are
      // added/removed.
      this.previewAreaResizeObserver.observe(
          (this.shadowRoot!.querySelector<HTMLDivElement>('#previewDiv')!));
    } else {
      window.removeEventListener('resize', this.onWindowResized);
      this.previewAreaResizeObserver.disconnect();
    }
  }

  /**
   * Make the scan progress height match the preview area height.
   *
   */
  private setMultiPageScanProgressHeight(): void {
    this.style.setProperty(
        '--multi-page-scan-progress-height',
        this.shadowRoot!.querySelector<HTMLDivElement>(
                            '#previewDiv')!.offsetHeight +
            'px');
  }

  private objectUrlsChanged(): void {
    if (!this.isMultiPageScan) {
      return;
    }

    // Set to -1 when no pages exist after a scan is saved.
    if (this.objectUrls.length === 0) {
      this.currentPageIndexInView = -1;
    }
  }

  /**
   * Sets the size and positioning of elements that depend on the size of the
   * scan preview area.
   *
   */
  private updatePreviewElements(): void {
    this.setMultiPageScanProgressHeight();
    this.setActionToolbarPosition();
  }

  /**
   * Hide the action toolbar if it's page is not currently in view.
   */
  private showActionToolbarByIndex(index: number): boolean {
    return index === this.currentPageIndexInView && this.showActionToolbar;
  }

  /**
   * Set |currentPageIndexInView_| to the page focused on (via ChromeVox).
   */
  private onScannedImageInFocus(e: DomRepeatEvent<string>): void {
    if (!this.isMultiPageScan) {
      return;
    }

    // |e.model| is populated by the dom-repeat element.
    const scannedImages =
        this.shadowRoot!.querySelector<HTMLDivElement>('#scannedImages')!
            .getElementsByClassName('scanned-image');
    this.setFocusedScannedImage(scannedImages, e.model.index);
  }

  private getScannedImageAriaLabel(index: number): string {
    return this.i18n(
        'multiPageImageAriaLabel', index + 1, this.objectUrls.length);
  }

  setIsMultiPageScanForTesting(isMultiPageScan: boolean): void {
    this.isMultiPageScan = isMultiPageScan;
  }

  setPageNumberForTesting(pageNumber: number): void {
    this.pageNumber = pageNumber;
  }
}

declare global {
  interface HTMLElementEventMap {
    'rescan-page': CustomEvent<number>;
    'remove-page': CustomEvent<number>;
  }

  interface HTMLElementTagNameMap {
    [ScanPreviewElement.is]: ScanPreviewElement;
  }
}

customElements.define(ScanPreviewElement.is, ScanPreviewElement);
