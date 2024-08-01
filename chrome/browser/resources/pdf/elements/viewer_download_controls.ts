// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SaveRequestType} from '../constants.js';

import {getCss} from './viewer_download_controls.css.js';
import {getHtml} from './viewer_download_controls.html.js';

export interface ViewerDownloadControlsElement {
  $: {
    download: CrIconButtonElement,
    menu: CrActionMenuElement,
  };
}

export class ViewerDownloadControlsElement extends CrLitElement {
  static get is() {
    return 'viewer-download-controls';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hasEdits: {type: Boolean},
      hasEnteredAnnotationMode: {type: Boolean},
      // <if expr="enable_pdf_ink2">
      hasInk2Edits: {type: Boolean},
      // </if>
      isFormFieldFocused: {type: Boolean},

      menuOpen_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  hasEdits: boolean = false;
  hasEnteredAnnotationMode: boolean = false;
  // <if expr="enable_pdf_ink2">
  hasInk2Edits: boolean = false;
  // </if>
  isFormFieldFocused: boolean = false;
  private menuOpen_: boolean = false;
  private waitForFormFocusChange_: PromiseResolver<boolean>|null = null;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('isFormFieldFocused') &&
        this.waitForFormFocusChange_ !== null) {
      // Resolving the promise in updated(), since this can trigger
      // showDownloadMenu_() which accesses the element's DOM.
      this.waitForFormFocusChange_.resolve(this.hasEdits);
      this.waitForFormFocusChange_ = null;
    }
  }

  isMenuOpen(): boolean {
    return this.menuOpen_;
  }

  closeMenu() {
    this.$.menu.close();
  }

  protected onOpenChanged_(e: CustomEvent<{value: boolean}>) {
    this.menuOpen_ = e.detail.value;
  }

  private hasEditsToSave_(): boolean {
    // <if expr="enable_pdf_ink2">
    return this.hasEnteredAnnotationMode || this.hasEdits || this.hasInk2Edits;
    // </if>
    // <if expr="not enable_pdf_ink2">
    return this.hasEnteredAnnotationMode || this.hasEdits;
    // </if>
  }

  /**
   * @return The value for the aria-haspopup attribute for the download button.
   */
  protected downloadHasPopup_(): string {
    return this.hasEditsToSave_() ? 'menu' : 'false';
  }

  private showDownloadMenu_() {
    this.$.menu.showAt(this.$.download, {
      anchorAlignmentX: AnchorAlignment.CENTER,
    });
    // For tests
    this.dispatchEvent(new CustomEvent(
        'download-menu-shown-for-testing', {bubbles: true, composed: true}));
  }

  protected onDownloadClick_() {
    this.waitForEdits_().then(hasEdits => {
      if (hasEdits) {
        this.showDownloadMenu_();
      } else {
        this.dispatchSaveEvent_(SaveRequestType.ORIGINAL);
      }
    });
  }

  /**
   * @return Promise that resolves with true if the PDF has edits and/or
   *     annotations, and false otherwise.
   */
  private waitForEdits_(): Promise<boolean> {
    if (this.hasEditsToSave_()) {
      return Promise.resolve(true);
    }
    if (!this.isFormFieldFocused) {
      return Promise.resolve(false);
    }
    this.waitForFormFocusChange_ = new PromiseResolver();
    return this.waitForFormFocusChange_.promise;
  }

  private dispatchSaveEvent_(type: SaveRequestType) {
    this.dispatchEvent(
        new CustomEvent('save', {detail: type, bubbles: true, composed: true}));
  }

  protected onDownloadOriginalClick_() {
    this.dispatchSaveEvent_(SaveRequestType.ORIGINAL);
    this.$.menu.close();
  }

  protected onDownloadEditedClick_() {
    this.dispatchSaveEvent_(
        this.hasEnteredAnnotationMode ? SaveRequestType.ANNOTATION :
                                        SaveRequestType.EDITED);
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-download-controls': ViewerDownloadControlsElement;
  }
}

customElements.define(
    ViewerDownloadControlsElement.is, ViewerDownloadControlsElement);
