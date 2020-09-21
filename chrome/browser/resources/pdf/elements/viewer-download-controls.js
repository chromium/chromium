// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './shared-css.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SaveRequestType} from '../constants.js';

export class ViewerDownloadControlsElement extends PolymerElement {
  static get is() {
    return 'viewer-download-controls';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      hasEdits: Boolean,

      hasEnteredAnnotationMode: Boolean,

      isFormFieldFocused: {
        type: Boolean,
        observer: 'onFormFieldFocusedChanged_',
      },

      pdfFormSaveEnabled: Boolean,

      downloadHasPopup_: {
        type: String,
        computed: 'computeDownloadHasPopup_(' +
            'pdfFormSaveEnabled, hasEdits, hasEnteredAnnotationMode)',
      },

      /** @private */
      menuOpen_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
    };
  }

  constructor() {
    super();

    // Polymer properties
    /** @private {string} */
    this.downloadHasPopup_;

    /** @type {boolean} */
    this.hasEdits;

    /** @type {boolean} */
    this.hasEnteredAnnotationMode;

    /** @type {boolean} */
    this.isFormFieldFocused;

    /** @type {boolean} */
    this.pdfFormSaveEnabled;

    // Non-Polymer properties
    /** @private {?PromiseResolver<boolean>} */
    this.waitForFormFocusChange_ = null;
  }

  /** @return {boolean} */
  isMenuOpen() {
    return this.menuOpen_;
  }

  closeMenu() {
    this.getDownloadMenu_().close();
  }

  /**
   * @param {!CustomEvent<!{value: boolean}>} e
   * @private
   */
  onOpenChanged_(e) {
    this.menuOpen_ = e.detail.value;
  }

  /**
   * @return {boolean}
   * @private
   */
  hasEditsToSave_() {
    return this.hasEnteredAnnotationMode ||
        (this.pdfFormSaveEnabled && this.hasEdits);
  }

  /**
   * @return {string} The value for the aria-haspopup attribute for the download
   *     button.
   * @private
   */
  computeDownloadHasPopup_() {
    return this.hasEditsToSave_() ? 'menu' : 'false';
  }

  /**
   * @return {!CrActionMenuElement}
   * @private
   */
  getDownloadMenu_() {
    return /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('#menu'));
  }

  /** @private */
  showDownloadMenu_() {
    this.getDownloadMenu_().showAt(this.$.download, {
      anchorAlignmentX: AnchorAlignment.CENTER,
    });
    // For tests
    this.dispatchEvent(new CustomEvent(
        'download-menu-shown-for-testing', {bubbles: true, composed: true}));
  }

  /** @private */
  onDownloadClick_() {
    this.waitForEdits_().then(hasEdits => {
      if (hasEdits) {
        this.showDownloadMenu_();
      } else {
        this.dispatchSaveEvent_(SaveRequestType.ORIGINAL);
      }
    });
  }

  /**
   * @return {!Promise<boolean>} Promise that resolves with true if the PDF has
   *     edits and/or annotations, and false otherwise.
   * @private
   */
  waitForEdits_() {
    if (this.hasEditsToSave_()) {
      return Promise.resolve(true);
    }
    if (!this.isFormFieldFocused || !this.pdfFormSaveEnabled) {
      return Promise.resolve(false);
    }
    this.waitForFormFocusChange_ = new PromiseResolver();
    return this.waitForFormFocusChange_.promise;
  }

  /** @private */
  onFormFieldFocusedChanged_() {
    if (!this.waitForFormFocusChange_) {
      return;
    }

    this.waitForFormFocusChange_.resolve(this.hasEdits);
    this.waitForFormFocusChange_ = null;
  }

  /**
   * @param {!SaveRequestType} type
   * @private
   */
  dispatchSaveEvent_(type) {
    this.dispatchEvent(
        new CustomEvent('save', {detail: type, bubbles: true, composed: true}));
  }

  /** @private */
  onDownloadOriginalClick_() {
    this.dispatchSaveEvent_(SaveRequestType.ORIGINAL);
    this.getDownloadMenu_().close();
  }

  /** @private */
  onDownloadEditedClick_() {
    this.dispatchSaveEvent_(
        this.hasEnteredAnnotationMode ? SaveRequestType.ANNOTATION :
                                        SaveRequestType.EDITED);
    this.getDownloadMenu_().close();
  }
}
customElements.define(
    ViewerDownloadControlsElement.is, ViewerDownloadControlsElement);
