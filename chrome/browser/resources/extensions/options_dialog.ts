// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {Debouncer, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';
import {getTemplate} from './options_dialog.html.js';

/**
 * @return A signal that the document is ready. Need to wait for this, otherwise
 *     the custom ExtensionOptions element might not have been registered yet.
 */
function whenDocumentReady(): Promise<void> {
  if (document.readyState === 'complete') {
    return Promise.resolve();
  }

  return new Promise<void>(function(resolve) {
    document.addEventListener('readystatechange', function f() {
      if (document.readyState === 'complete') {
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

export interface ExtensionsOptionsDialogElement {
  $: {
    body: HTMLElement,
    dialog: CrDialogElement,
  };
}

export class ExtensionsOptionsDialogElement extends PolymerElement {
  static get is() {
    return 'extensions-options-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionOptions_: Object,
      data_: Object,
    };
  }

  private extensionOptions_: any;
  private data_: chrome.developerPrivate.ExtensionInfo;
  private preferredSize_: {height: number, width: number}|null = null;
  private debouncer_: Debouncer|null = null;
  private eventTracker_: EventTracker = new EventTracker();

  get open() {
    return this.$.dialog.open;
  }

  /**
   * Resizes the dialog to the width/height stored in |preferredSize_|, taking
   * into account the window width/height.
   */
  private updateDialogSize_() {
    let headerHeight = this.$.body.offsetTop;
    if (this.$.body.assignedSlot && this.$.body.assignedSlot.parentElement) {
      headerHeight = this.$.body.assignedSlot.parentElement.offsetTop;
    }
    const maxHeight =
        Math.min(0.9 * window.innerHeight, OptionsDialogMaxHeight);
    const effectiveHeight =
        Math.min(maxHeight, headerHeight + this.preferredSize_!.height);
    const effectiveWidth =
        Math.max(OptionsDialogMinWidth, this.preferredSize_!.width);

    this.$.dialog.style.setProperty('--dialog-height', `${effectiveHeight}px`);
    this.$.dialog.style.setProperty('--dialog-width', `${effectiveWidth}px`);
    this.$.dialog.style.setProperty('--dialog-opacity', '1');
  }

  show(data: chrome.developerPrivate.ExtensionInfo) {
    this.data_ = data;
    whenDocumentReady().then(() => {
      if (!this.extensionOptions_) {
        this.extensionOptions_ = document.createElement('ExtensionOptions');
      }
      this.extensionOptions_.extension = this.data_.id;
      this.extensionOptions_.onclose = () => this.$.dialog.close();

      const boundUpdateDialogSize = this.updateDialogSize_.bind(this);
      this.extensionOptions_.onpreferredsizechanged =
          (e: {height: number, width: number}) => {
            if (!this.$.dialog.open) {
              this.$.dialog.showModal();
            }
            this.preferredSize_ = e;
            this.debouncer_ = Debouncer.debounce(
                this.debouncer_, timeOut.after(50), boundUpdateDialogSize);
          };

      // Add a 'resize' such that the dialog is resized when window size
      // changes.
      this.eventTracker_.add(window, 'resize', boundUpdateDialogSize);
      this.$.body.appendChild(this.extensionOptions_);
    });
  }

  private onClose_() {
    this.extensionOptions_.onpreferredsizechanged = null;
    this.eventTracker_.removeAll();

    const currentPage = navigation.getCurrentPage();
    // We update the page when the options dialog closes, but only if we're
    // still on the details page. We could be on a different page if the
    // user hit back while the options dialog was visible; in that case, the
    // new page is already correct.
    if (currentPage && currentPage.page === Page.DETAILS) {
      // This will update the currentPage_ and the NavigationHelper; since
      // the active page is already the details page, no main page
      // transition occurs.
      navigation.navigateTo(
          {page: Page.DETAILS, extensionId: currentPage.extensionId});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-options-dialog': ExtensionsOptionsDialogElement;
  }
}

customElements.define(
    ExtensionsOptionsDialogElement.is, ExtensionsOptionsDialogElement);
