// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AttachedFile} from './feedback_types.js';

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
        computed: 'computeHasSelectedAFile_(selectedFile_)',
      },
    };
  }

  constructor() {
    super();

    /**
     * The file selected if any to be attached to the report.
     * @type {?File}
     * @private
     */
    this.selectedFile_ = null;

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
    return !!this.selectedFile_;
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
   * Gather the file name and data chosen.
   * @return {!Promise<?AttachedFile>}
   */
  async getAttachedFile() {
    if (!this.getElement_('#selectFileCheckbox').checked) {
      return null;
    }
    if (!this.selectedFile_) {
      return null;
    }

    const fileDataBuffer = await this.selectedFile_.arrayBuffer();
    const fileDataView = new Uint8Array(fileDataBuffer);
    // fileData is of type BigBuffer which can take byte array format or
    // shared memory form. For now, byte array is being used for its simplicity.
    // For better performance, we may switch to shared memory.
    const fileData = {bytes: Array.from(fileDataView)};

    /** @type {!AttachedFile} */
    const attachedFile = {
      fileName: {path: {path: this.selectedFile_.name}},
      fileData: fileData
    };

    return attachedFile;
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
      this.handleSelectedFileHelper_(fileInput.files[0]);
    }
  }

  /**
   * @param {!File} file
   * @private
   */
  handleSelectedFileHelper_(file) {
    // Maximum file size is 10MB.
    const MAX_ATTACH_FILE_SIZE_BYTES = 10 * 1024 * 1024;
    if (file.size > MAX_ATTACH_FILE_SIZE_BYTES) {
      this.getElement_('#fileTooBigErrorMessage').show();
      return;
    }
    this.selectedFile_ = file;
    this.getElement_('#selectedFileName').textContent = file.name;
    this.getElement_('#selectFileCheckbox').checked = true;
  }

  /**
   * @param {!File} file
   */
  setSelectedFileForTesting(file) {
    this.handleSelectedFileHelper_(file);
  }
}

customElements.define(FileAttachmentElement.is, FileAttachmentElement);
