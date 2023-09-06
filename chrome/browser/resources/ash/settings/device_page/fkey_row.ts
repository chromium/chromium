// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'fkey-row' displays an fKey alongside a dropdown menu that allows users to
 * set a shortcut for remapping key events to F11/F12.
 */

import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './fkey_row.html.js';

export class FkeyRowElement extends PolymerElement {
  static get is() {
    return 'fkey-row' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FkeyRowElement.is]: FkeyRowElement;
  }
}

customElements.define(FkeyRowElement.is, FkeyRowElement);
