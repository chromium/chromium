// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'keyboard-six-pack-key-row' displays a six pack key alongside a dropdown
 * menu that allows users to set the shortcut that triggers the corresponding
 * six pack key action.
 */

import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './keyboard_six_pack_key_row.html.js';

export class KeyboardSixPackKeyRowElement extends PolymerElement {
  static get is() {
    return 'keyboard-six-pack-key-row' as const;
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
    [KeyboardSixPackKeyRowElement.is]: KeyboardSixPackKeyRowElement;
  }
}

customElements.define(
    KeyboardSixPackKeyRowElement.is, KeyboardSixPackKeyRowElement);
