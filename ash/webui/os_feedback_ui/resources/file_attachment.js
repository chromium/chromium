// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import './os_feedback_shared_css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {stringToMojoString16} from 'chrome://resources/ash/common/mojo_utils.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AttachedFile, FeedbackAppPreSubmitAction, FeedbackServiceProviderInterface} from './feedback_types.js';
import {getFeedbackServiceProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'file-attachment' allows users to select a file as an attachment to the
 *  report.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const FileAttachmentElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class FileAttachmentElement extends FileAttachmentElementBase {
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
     * The name of the file selected
     * @type {string}
     * @protected
     */
    this.selectedFileName_;

    /**
     * Url of the selected image.
     * @type {string}
     */
    this.selectedImageUrl_;

    /**
     * True when there is a file selected.
     * @protected {boolean}
     */
    this.hasSelectedAFile_;

    /** @private {!FeedbackServiceProviderInterface} */
    this.feedbackServiceProvider_ = getFeedbackServiceProvider();
  }

  ready() {
    super.ready();
    // Set the aria description works the best for screen reader.
    // It reads the description when the checkbox is focused, and when it is
    // checked and unchecked.
    this.$.selectFileCheckbox.ariaDescription =
        this.i18n('attachFileCheckboxArialLabel');
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
      fileData: fileData,
    };

    return attachedFile;
  }

  /**
   * Get the image url when uploaded file is image type.
   * @param {!File} file
   * @return {!Promise<string>}
   * @private
   */
  async getImageUrl_(file) {
    const fileDataBuffer = await file.arrayBuffer();
    const fileDataView = new Uint8Array(fileDataBuffer);
    const blob = new Blob([Uint8Array.from(fileDataView)], {type: file.type});

    const imageUrl = URL.createObjectURL(blob);
    return imageUrl;
  }

  /** @protected */
  handleSelectedImageClick_() {
    this.$.selectedImageDialog.showModal();
    this.feedbackServiceProvider_.recordPreSubmitAction(
        FeedbackAppPreSubmitAction.kViewedImage);
  }

  /** @protected */
  handleSelectedImageDialogCloseClick_() {
    this.$.selectedImageDialog.close();
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleOpenFileInputClick_(e) {
    e.preventDefault();
    const fileInput = this.getElement_('#selectFileDialog');
    // Clear the value so that when the user selects the same file again, the
    // change event will be triggered. Otherwise, if the file size exceeds the
    // limit, the error alert will not be displayed when the user selects the
    // same file again.
    fileInput.value = null;
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
    assert(file);
    // Maximum file size is 10MB.
    const MAX_ATTACH_FILE_SIZE_BYTES = 10 * 1024 * 1024;
    if (file.size > MAX_ATTACH_FILE_SIZE_BYTES) {
      this.getElement_('#fileTooBigErrorMessage').show();
      return;
    }
    this.selectedFile_ = file;
    this.selectedFileName_ = file.name;
    this.getElement_('#selectFileCheckbox').checked = true;

    // Add a preview image when selected file is image type.
    if (file.type.startsWith('image/')) {
      this.getImageUrl_(file).then((imageUrl) => {
        this.selectedImageUrl_ = imageUrl;
        this.$.selectedImageButton.ariaLabel =
            this.i18n('previewImageAriaLabel', file.name);
      });
    } else {
      this.selectedImageUrl_ = '';
      this.$.selectedImageButton.ariaLabel = '';
    }
  }

  /**
   * @param {!File} file
   */
  setSelectedFileForTesting(file) {
    this.handleSelectedFileHelper_(file);
  }
}

customElements.define(FileAttachmentElement.is, FileAttachmentElement);
