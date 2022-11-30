// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './lens_form.html.js';

/** Lens service endpoint for the Upload by File action. */
const UPLOAD_FILE_ACTION = 'https://lens.google.com/upload';

const SUPPORTED_FILE_TYPES = [
  'image/bmp',
  'image/heic',
  'image/heif',
  'image/jpeg',
  'image/png',
  'image/tiff',
  'image/webp',
  'image/x-icon',
];

/** Maximum file size support by Lens in bytes. */
const MAX_FILE_SIZE_BYTES = 20 * 1024 * 1024;  // 20MB

export enum LensErrorType {
  // The user attempted to upload multiple files at the same time.
  MULTIPLE_FILES,
  // The user didn't provide a file.
  NO_FILE,
  // The user provided a file type that is not supported.
  FILE_TYPE,
  // The user provided a file that is too large.
  FILE_SIZE,
}

export interface LensFormElement {
  $: {
    fileForm: HTMLFormElement,
    fileInput: HTMLInputElement,
  };
}

export class LensFormElement extends PolymerElement {
  static get is() {
    return 'ntp-lens-form';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      supportedFileTypes_: {
        type: String,
        readOnly: true,
        value: SUPPORTED_FILE_TYPES.join(','),
      },
      uploadFileAction_: {
        type: String,
        readOnly: true,
        value: UPLOAD_FILE_ACTION,
      },
    };
  }

  openSystemFilePicker() {
    this.$.fileInput.click();
  }

  private handleFileInputChange_() {
    const fileList = this.$.fileInput.files;
    if (fileList) {
      this.submitFileList_(fileList);
    }
  }

  private submitFileList_(files: FileList) {
    if (files.length > 1) {
      this.dispatchError_(LensErrorType.MULTIPLE_FILES);
      return;
    }
    const file = files[0];

    if (!file) {
      this.dispatchError_(LensErrorType.NO_FILE);
      return;
    }
    return this.submitFile_(file);
  }

  private submitFile_(file: File) {
    if (!SUPPORTED_FILE_TYPES.includes(file.type)) {
      this.dispatchError_(LensErrorType.FILE_TYPE);
      return;
    }

    if (file.size > MAX_FILE_SIZE_BYTES) {
      this.dispatchError_(LensErrorType.FILE_SIZE);
      return;
    }

    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    this.$.fileInput.files = dataTransfer.files;

    this.dispatchEvent(new Event('loading'));
    this.$.fileForm.submit();
  }

  private dispatchError_(errorType: LensErrorType) {
    this.dispatchEvent(new CustomEvent('error', {
      bubbles: false,
      composed: false,
      detail: errorType,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-lens-form': LensFormElement;
  }
}

customElements.define(LensFormElement.is, LensFormElement);
