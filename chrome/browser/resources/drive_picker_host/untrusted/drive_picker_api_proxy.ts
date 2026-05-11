// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome-untrusted://resources/js/assert.js';
import {PromiseResolver} from 'chrome-untrusted://resources/js/promise_resolver.js';
import {getTrustedScriptURL} from 'chrome-untrusted://resources/js/static_types.js';

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
  ensureGapiLoaded(): Promise<void>;
  showPicker(
      oauthToken: string, apiKey: string, appId: string,
      callback: (data: GooglePickerResponse) => void): Promise<void>;
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

  ensureGapiLoaded(): Promise<void> {
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

  async showPicker(
      oauthToken: string, apiKey: string, appId: string,
      callback: (data: GooglePickerResponse) => void): Promise<void> {
    await this.ensureGapiLoaded();

    const view = new google.picker.DocsView(google.picker.ViewId.DOCS);
    const picker = new google.picker.PickerBuilder()
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
                       .setCallback(callback)
                       .build();
    picker.setVisible(true);
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
