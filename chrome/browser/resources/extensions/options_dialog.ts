// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {navigation, Page} from './navigation_helper.js';
import {getCss} from './options_dialog.css.js';
import {getHtml} from './options_dialog.html.js';

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

interface ExtensionOptionsElement extends HTMLElement {
  extension: string;
  onclose: () => void;
  onpreferredsizechanged: ((e: {height: number, width: number}) => void)|null;
}

export interface ExtensionsOptionsDialogElement {
  $: {
    body: HTMLElement,
    dialog: CrDialogElement,
  };
}

export class ExtensionsOptionsDialogElement extends CrLitElement {
  static get is() {
    return 'extensions-options-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensionOptions_: {type: Object},
      data_: {type: Object},
    };
  }

  private extensionOptions_?: ExtensionOptionsElement;
  protected data_?: chrome.developerPrivate.ExtensionInfo;
  private preferredSize_: {height: number, width: number}|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private updateDialogSizeTimeout_: number|null = null;

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
        this.extensionOptions_ = document.createElement('ExtensionOptions') as
            ExtensionOptionsElement;
      }
      this.extensionOptions_.extension = this.data_!.id;
      this.extensionOptions_.onclose = () => this.$.dialog.close();

      this.extensionOptions_.onpreferredsizechanged =
          (e: {height: number, width: number}) => {
            if (!this.$.dialog.open) {
              this.$.dialog.showModal();
            }
            this.preferredSize_ = e;
            if (this.updateDialogSizeTimeout_) {
              clearTimeout(this.updateDialogSizeTimeout_);
            }
            this.updateDialogSizeTimeout_ = setTimeout(() => {
              this.updateDialogSizeTimeout_ = null;
              this.updateDialogSize_();
            }, 50);
          };

      // Add a 'resize' such that the dialog is resized when window size
      // changes.
      const boundUpdateDialogSize = this.updateDialogSize_.bind(this);
      this.eventTracker_.add(window, 'resize', boundUpdateDialogSize);
      this.$.body.appendChild(this.extensionOptions_);
    });
  }

  protected onClose_() {
    assert(this.extensionOptions_);
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

// Exported to be used in the autogenerated Lit template file
export type OptionsDialogElement = ExtensionsOptionsDialogElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-options-dialog': ExtensionsOptionsDialogElement;
  }
}

customElements.define(
    ExtensionsOptionsDialogElement.is, ExtensionsOptionsDialogElement);
