// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from './i18n_setup.js';
import {getTemplate} from './lens_form.html.js';

/** Lens service endpoint for the Upload by File action. */
const UPLOAD_FILE_ACTION: string = 'https://lens.google.com/upload';

/** Entrypoint for the upload by file action. */
const UPLOAD_FILE_ENTRYPOINT: string = 'cntpubb';

/** Lens service endpoint for the Upload by URL action. */
const UPLOAD_BY_URL_ACTION: string = 'https://lens.google.com/uploadbyurl';

/** Entrypoint for the upload by url action. */
const UPLOAD_URL_ENTRYPOINT: string = 'cntpubu';

/** Rendering environment for the NTP searchbox entrypoint. */
const RENDERING_ENVIRONMENT: string = 'df';

/** The value of Surface.CHROMIUM expected by Lens Web. */
const CHROMIUM_SURFACE: string = '4';

/** Max length for encoded input URL. */
const MAX_URL_LENGTH: number = 2000;

const SUPPORTED_FILE_TYPES: string[] = [
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
const MAX_FILE_SIZE_BYTES: number = 20 * 1024 * 1024;  // 20MB

export enum LensErrorType {
  // The user attempted to upload multiple files at the same time.
  MULTIPLE_FILES,
  // The user didn't provide a file.
  NO_FILE,
  // The user provided a file type that is not supported.
  FILE_TYPE,
  // The user provided a file that is too large.
  FILE_SIZE,
  // The user provided a url with an invalid or missing scheme.
  INVALID_SCHEME,
  // The user provided a string that does not parse to a valid url.
  INVALID_URL,
  // The user provided a string that was too long.
  LENGTH_TOO_GREAT,
}

export enum LensSubmitType {
  FILE,
  URL,
}

export interface LensFormElement {
  $: {
    fileForm: HTMLFormElement,
    fileInput: HTMLInputElement,
    urlForm: HTMLFormElement,
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
      renderingEnvironment_: {
        type: String,
        readOnly: true,
        value: RENDERING_ENVIRONMENT,
      },
      chromiumSurface_: {
        type: String,
        readOnly: true,
        value: CHROMIUM_SURFACE,
      },
      uploadFileAction_: String,
      uploadUrlAction_: {
        type: String,
        readOnly: true,
        value: UPLOAD_BY_URL_ACTION,
      },
      uploadUrl_: String,
      uploadUrlEntrypoint_: {
        type: String,
        readOnly: true,
        value: UPLOAD_URL_ENTRYPOINT,
      },
      language_: {
        type: String,
        readOnly: true,
        value: window.navigator.language,
      },
      clientData_: {
        type: String,
        readOnly: true,
        value: loadTimeData.getString('realboxLensVariations'),
      },
    };
  }

  private language_: string;
  private uploadFileAction_: string = UPLOAD_FILE_ACTION;
  private uploadUrl_: string = '';
  private startTime_: string|null = null;
  private clientData_: string;

  openSystemFilePicker() {
    this.$.fileInput.click();
  }

  private handleFileInputChange_() {
    const fileList = this.$.fileInput.files;
    if (fileList) {
      this.submitFileList(fileList);
    }
  }

  submitFileList(files: FileList) {
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

    this.startTime_ = Date.now().toString();

    const action = new URL(UPLOAD_FILE_ACTION);
    action.searchParams.set('ep', UPLOAD_FILE_ENTRYPOINT);
    action.searchParams.set('hl', this.language_);
    action.searchParams.set('st', this.startTime_.toString());
    action.searchParams.set('cd', this.clientData_);
    action.searchParams.set('re', RENDERING_ENVIRONMENT);
    action.searchParams.set('s', CHROMIUM_SURFACE);
    this.uploadFileAction_ = action.toString();

    this.dispatchLoading_(LensSubmitType.FILE);
    this.$.fileForm.submit();
  }

  submitUrl(urlString: string) {
    if (!urlString.startsWith('http://') && !urlString.startsWith('https://')) {
      this.dispatchError_(LensErrorType.INVALID_SCHEME);
      return;
    }

    let encodedUri: string;
    try {
      encodedUri = encodeURI(urlString);
      new URL(urlString);  // Throws an error if fails to parse.
    } catch (e) {
      this.dispatchError_(LensErrorType.INVALID_URL);
      return;
    }

    if (encodedUri.length > MAX_URL_LENGTH) {
      this.dispatchError_(LensErrorType.LENGTH_TOO_GREAT);
      return;
    }

    this.uploadUrl_ = encodedUri;
    this.startTime_ = Date.now().toString();
    this.dispatchLoading_(LensSubmitType.URL);
    this.$.urlForm.submit();
  }

  private dispatchLoading_(submitType: LensSubmitType) {
    this.dispatchEvent(new CustomEvent('loading', {
      bubbles: false,
      composed: false,
      detail: submitType,
    }));
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
