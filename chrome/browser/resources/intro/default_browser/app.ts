// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/tangible_sync_style_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

export class DefaultBrowserAppElement extends PolymerElement {
  static get is() {
    return 'default-browser-app';
  }

  static get template() {
    return getTemplate();
  }

  private onConfirmDefaultBrowserClick_() {
    // TODO(crbug.com/1465822): Implement button action.
  }

  private onSkipDefaultBrowserClick_() {
    // TODO(crbug.com/1465822): Implement button action.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'default-browser-app': DefaultBrowserAppElement;
  }
}

customElements.define(DefaultBrowserAppElement.is, DefaultBrowserAppElement);
