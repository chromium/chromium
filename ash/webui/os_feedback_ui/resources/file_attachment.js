// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'file-attachment' allows users to select a file as an attachment to the
 *  report.
 */
export class FileAttachmentElement extends PolymerElement {
  static get is() {
    return 'file-attachment';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      hasSelectedAFile_: {
        type: Boolean,
        computed: 'computeHasSelectedAFile_(selectedFile)',
      },

      selectedFile: {
        type: File,
        notify: true,
        readOnly: true,
        reflectToAttribute: true,
      },
    };
  }

  constructor() {
    super();

    /**
     * The file selected if any to be attached to the report.
     * @type {?File}
     */
    this.selectedFile = null;

    /**
     * True when there is a file selected.
     * @protected {boolean}
     */
    this.hasSelectedAFile_;
  }

  /**
   * @returns {boolean}
   * @private
   */
  computeHasSelectedAFile_() {
    return !!this.selectedFile;
  }

  /**
   * @param {string} selector
   * @return {?Element}
   * @private
   */
  getElement_(selector) {
    return this.shadowRoot.querySelector(selector);
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleOpenFileInputClick_(e) {
    const fileInput = this.getElement_('#selectFileDialog');
    fileInput.click();
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleFileSelectChange_(e) {
    const fileInput = /**@type {HTMLInputElement} */ (e.target);
    // The feedback app takes maximum one attachment. And the file dialog is set
    // to accept one file only.
    if (fileInput.files.length > 0) {
      const selectedFileName = this.getElement_('#selectedFileName');
      selectedFileName.textContent = fileInput.files[0].name;
      this.getElement_('#selectFileCheckbox').checked = true;
      this.selectedFile = fileInput.files[0];
    }
  }

  /**
   * @param {!File} file
   */
  setSelectedFileForTesting(file) {
    this.selectedFile = file;
  }
}

customElements.define(FileAttachmentElement.is, FileAttachmentElement);
