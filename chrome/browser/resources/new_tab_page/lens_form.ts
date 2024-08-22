// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
import type {ProcessedFile} from './image_processor.js';
import {processFile, SUPPORTED_FILE_TYPES} from './image_processor.js';
import {getCss} from './lens_form.css.js';
import {getHtml} from './lens_form.html.js';

/** Lens service endpoint for the Upload by File action. */
const SCOTTY_UPLOAD_FILE_ACTION: string = 'https://lens.google.com/upload';
const DIRECT_UPLOAD_FILE_ACTION: string = 'https://lens.google.com/v3/upload';

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

export class LensFormElement extends CrLitElement {
  static get is() {
    return 'ntp-lens-form';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      supportedFileTypes_: {type: String},
      renderingEnvironment_: {type: String},
      chromiumSurface_: {type: String},
      useDirectUpload_: {type: Boolean},
      uploadFileAction_: {type: String},
      uploadUrlAction_: {type: String},
      uploadUrl_: {type: String},
      uploadUrlEntrypoint_: {type: String},
      language_: {type: String},
      clientData_: {type: String},
      startTime_: {type: String},
    };
  }

  protected supportedFileTypes_: string = SUPPORTED_FILE_TYPES.join(',');
  protected renderingEnvironment_: string = RENDERING_ENVIRONMENT;
  protected chromiumSurface_: string = CHROMIUM_SURFACE;
  protected language_: string = window.navigator.language;
  protected uploadFileAction_: string = SCOTTY_UPLOAD_FILE_ACTION;
  protected uploadUrlAction_: string = UPLOAD_BY_URL_ACTION;
  protected uploadUrl_: string = '';
  protected uploadUrlEntrypoint_: string = UPLOAD_URL_ENTRYPOINT;
  protected startTime_: string|null = null;
  protected clientData_: string =
      loadTimeData.getString('searchboxLensVariations');
  private useDirectUpload_: boolean =
      loadTimeData.getBoolean('searchboxLensDirectUpload');

  openSystemFilePicker() {
    this.$.fileInput.click();
  }

  protected handleFileInputChange_() {
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

  private async submitFile_(file: File) {
    if (!SUPPORTED_FILE_TYPES.includes(file.type)) {
      this.dispatchError_(LensErrorType.FILE_TYPE);
      return;
    }

    if (file.size > MAX_FILE_SIZE_BYTES) {
      this.dispatchError_(LensErrorType.FILE_SIZE);
      return;
    }

    if (this.useDirectUpload_) {
      this.uploadFileAction_ = DIRECT_UPLOAD_FILE_ACTION;
    }

    this.startTime_ = Date.now().toString();

    let processedFile: ProcessedFile = {processedFile: file};

    if (this.useDirectUpload_) {
      processedFile = await processFile(file);
    }

    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(processedFile.processedFile);
    this.$.fileInput.files = dataTransfer.files;

    const action = new URL(this.uploadFileAction_);
    action.searchParams.set('ep', UPLOAD_FILE_ENTRYPOINT);
    action.searchParams.set('hl', this.language_);
    action.searchParams.set('st', this.startTime_.toString());
    action.searchParams.set('cd', this.clientData_);
    action.searchParams.set('re', RENDERING_ENVIRONMENT);
    action.searchParams.set('s', CHROMIUM_SURFACE);
    action.searchParams.set(
        'vph',
        processedFile.imageHeight ? processedFile.imageHeight.toString() : '');
    action.searchParams.set(
        'vpw',
        processedFile.imageWidth ? processedFile.imageWidth.toString() : '');
    this.uploadFileAction_ = action.toString();

    await this.updateComplete;
    this.dispatchLoading_(LensSubmitType.FILE);
    this.$.fileForm.submit();
  }

  async submitUrl(urlString: string) {
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
    await this.updateComplete;
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
