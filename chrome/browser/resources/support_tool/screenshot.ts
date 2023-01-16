// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './screenshot.html.js';
import {SupportToolPageMixin} from './support_tool_page_mixin.js';

const ScreenshotElementBase = SupportToolPageMixin(PolymerElement);

export class ScreenshotElement extends ScreenshotElementBase {
  static get is() {
    return 'screenshot-element';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hasScreenshotPreview_: {
        type: Boolean,
        value: false,
      },
      screenshotBase64_: {
        type: String,
        value: '',
      },
    };
  }

  private screenshotBase64_: string = '';
  private hasScreenshotPreview_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  setScreenshotData(dataBase64: string) {
    this.screenshotBase64_ = dataBase64;
    this.hasScreenshotPreview_ = true;
  }

  getEditedScreenshotBase64() {
    return this.screenshotBase64_;
  }

  private onTakeScreenshotClick_() {
    this.browserProxy_.takeScreenshot();
  }

  private onRemoveScreenshotClick_() {
    this.hasScreenshotPreview_ = false;
    this.screenshotBase64_ = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'screenshot-element': ScreenshotElement;
  }
}

customElements.define(ScreenshotElement.is, ScreenshotElement);
