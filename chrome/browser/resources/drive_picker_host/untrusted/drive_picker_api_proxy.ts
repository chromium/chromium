// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome-untrusted://resources/js/assert.js';
import {PromiseResolver} from 'chrome-untrusted://resources/js/promise_resolver.js';
import {getTrustedScriptURL} from 'chrome-untrusted://resources/js/static_types.js';

import {DrivePickerError} from './drive_picker_result_handler.mojom-webui.js';
import {DrivePickerSanitizer, SanitizationError} from './sanitizer.js';
import type {SanitizedDriveFile} from './sanitizer.js';

/**
 * Minimal interface for the Google API Loader.
 */
export interface Gapi {
  load(api: string, options: {callback: () => void, onerror?: () => void}):
      void;
}

/**
 * Minimal interface for the Google Picker API.
 * See https://developers.google.com/picker/docs/reference
 *  TODO(crbug.com/510492475): Migrate to types from third_party/node
 * Remove these manually copied type definitions.
 */
export interface GooglePicker {
  DocsView: {new(viewId: string): GooglePickerView};
  PickerBuilder: {new(): GooglePickerBuilder};
  ViewId: {DOCS: string};
  Feature: {MULTISELECT_ENABLED: string};
  Action: {PICKED: string, CANCEL: string};
  Response: {ACTION: string, DOCUMENTS: string};
  Document: {
    ID: string,
    MIME_TYPE: string,
    NAME: string,
    TYPE: string,
    SIZE_BYTES: string,
    RESOURCE_KEY: string,
    THUMBNAIL_URL: string,
  };
  Type: {
    DOCUMENT: string,
    PHOTO: string,
    VIDEO: string,
  };
}

export interface GooglePickerView {
  setMimeTypes(mimeTypes: string): GooglePickerView;
}

export interface GooglePickerBuilder {
  addView(view: GooglePickerView): GooglePickerBuilder;
  setOAuthToken(token: string): GooglePickerBuilder;
  setDeveloperKey(key: string): GooglePickerBuilder;
  setAppId(appId: string): GooglePickerBuilder;
  setOrigin(origin: string): GooglePickerBuilder;
  enableFeature(feature: string): GooglePickerBuilder;
  setCallback(callback: (data: GooglePickerResponse) => void):
      GooglePickerBuilder;
  build(): GooglePickerInstance;
}

export interface GooglePickerInstance {
  setVisible(visible: boolean): void;
}

export interface GooglePickerResponse {
  [key: string]: unknown;
}

export interface DrivePickerApiProxy {
  showPicker(oauthToken: string, apiKey: string, appId: string):
      Promise<SanitizedDriveFile[]|'CANCEL'>;
}

export class DrivePickerApiProxyImpl implements DrivePickerApiProxy {
  private gapiLoadResolver_: PromiseResolver<void>|null = null;

  private onError_() {
    if (this.gapiLoadResolver_) {
      this.gapiLoadResolver_.reject();
      this.gapiLoadResolver_ = null;
    }
  }

  private loadPicker_() {
    window.gapi.load('picker', {
      callback: () => {
        assert(this.gapiLoadResolver_);
        this.gapiLoadResolver_.resolve();
      },
      onerror: () => {
        this.onError_();
      },
    });
  }
  private pickerActive_ = false;

  private ensureGapiLoaded_(): Promise<void> {
    if (this.gapiLoadResolver_) {
      return this.gapiLoadResolver_.promise;
    }

    this.gapiLoadResolver_ = new PromiseResolver<void>();

    if (window.gapi) {
      this.gapiLoadResolver_.resolve();
      return this.gapiLoadResolver_.promise;
    }

    if (!window.gapi) {
      const script = document.createElement('script');
      script.type = 'text/javascript';
      script.src = getTrustedScriptURL`https://apis.google.com/js/api.js`;
      script.onerror = () => {
        this.onError_();
      };
      script.onload = () => {
        this.loadPicker_();
      };
      document.head.appendChild(script);
    } else {
      this.loadPicker_();
    }

    return this.gapiLoadResolver_.promise;
  }

  async showPicker(oauthToken: string, apiKey: string, appId: string):
      Promise<SanitizedDriveFile[]|'CANCEL'> {
    if (this.pickerActive_) {
      throw DrivePickerError.kAlreadyActive;
    }

    try {
      await this.ensureGapiLoaded_();
    } catch (e) {
      throw DrivePickerError.kApiLoadFailure;
    }

    return new Promise((resolve, reject) => {
      this.pickerActive_ = true;

      const view = new google.picker.DocsView(google.picker.ViewId.DOCS);
      const picker =
          new google.picker.PickerBuilder()
              .addView(view)
              .setOAuthToken(oauthToken)
              .setDeveloperKey(apiKey)
              .setAppId(appId)
              // The origin is set to chrome://drive-picker-host to
              // match the trusted host's origin. This is required for
              // Drive Picker to appear since untrusted content is
              // blocked by Chrome.
              .setOrigin('chrome://drive-picker-host')
              .enableFeature(google.picker.Feature.MULTISELECT_ENABLED)
              .setCallback((data: GooglePickerResponse) => {
                const action = data[google.picker.Response.ACTION];

                if (action === google.picker.Action.CANCEL) {
                  this.pickerActive_ = false;
                  resolve('CANCEL');
                  return;
                }

                if (action === google.picker.Action.PICKED) {
                  this.pickerActive_ = false;
                  try {
                    resolve(this.handleSelection_(data));
                  } catch (e) {
                    reject(e);
                  }
                }
              })
              .build();
      picker.setVisible(true);
    });
  }

  private handleSelection_(data: GooglePickerResponse): SanitizedDriveFile[] {
    const documents = data[google.picker.Response.DOCUMENTS];
    if (!Array.isArray(documents)) {
      throw DrivePickerError.kInvalidResponse;
    }

    const sanitizedFiles: SanitizedDriveFile[] = [];
    const pickerKeys = {
      ID: google.picker.Document.ID,
      MIME_TYPE: google.picker.Document.MIME_TYPE,
      NAME: google.picker.Document.NAME,
      TYPE: google.picker.Document.TYPE,
      SIZE_BYTES: google.picker.Document.SIZE_BYTES,
      RESOURCE_KEY: google.picker.Document.RESOURCE_KEY,
      THUMBNAIL_URL: google.picker.Document.THUMBNAIL_URL,
    };
    const allowedTypes = new Set([
      google.picker.Type.DOCUMENT,
      google.picker.Type.PHOTO,
      google.picker.Type.VIDEO,
    ]);

    for (const doc of documents) {
      try {
        const sanitized = DrivePickerSanitizer.sanitizeDocument(
            doc as Record<string, unknown>, pickerKeys, allowedTypes);
        sanitizedFiles.push(sanitized);
      } catch (e) {
        if (e instanceof Error) {
          const errorCode = Number(e.message);
          throw this.mapSanitizationError_(errorCode);
        } else {
          throw DrivePickerError.kInvalidResponse;
        }
      }
    }

    return sanitizedFiles;
  }

  private mapSanitizationError_(error: SanitizationError): DrivePickerError {
    switch (error) {
      case SanitizationError.INVALID_FILE_ID:
        return DrivePickerError.kInvalidFileId;
      case SanitizationError.INVALID_FILE_METADATA:
        return DrivePickerError.kInvalidFileMetadata;
      case SanitizationError.UNSUPPORTED_FILE_TYPE:
        return DrivePickerError.kUnsupportedFileType;
      case SanitizationError.INVALID_FILE_SIZE:
        return DrivePickerError.kInvalidFileSize;
      default:
        return DrivePickerError.kInvalidResponse;
    }
  }

  static getInstance(): DrivePickerApiProxy {
    return instance || (instance = new DrivePickerApiProxyImpl());
  }

  static setInstance(obj: DrivePickerApiProxy) {
    instance = obj;
  }
}

let instance: DrivePickerApiProxy|null = null;

declare global {
  namespace google {
    const picker: GooglePicker;
  }
  interface Window {
    gapi: Gapi;
  }
}
