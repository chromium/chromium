// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bottom_nav_content.html.js';

/**
 * @fileoverview
 * ShortcutsBottomNavContentElement contains the keyboard settings link and
 * (when customization is enabled) the button to restore all shortcuts to
 * default.
 */
const ShortcutsBottomNavContentElementBase = I18nMixin(PolymerElement);

export class ShortcutsBottomNavContentElement extends
    ShortcutsBottomNavContentElementBase {
  static get is(): string {
    return 'shortcuts-bottom-nav-content';
  }

  static get properties(): PolymerElementProperties {
    return {
      restoreAllButtonHidden: {
        type: Boolean,
        value: false,
      },
      keyboardSettingsLink: {
        type: String,
        value: '',
      },
    };
  }

  restoreAllButtonHidden: boolean;
  keyboardSettingsLink: string;

  onRestoreAllDefaultClicked(): void {
    this.dispatchEvent(new CustomEvent('restore-all-default', {
      bubbles: true,
      composed: true,
    }));
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shortcuts-bottom-nav-content': ShortcutsBottomNavContentElement;
  }
}

customElements.define(
    ShortcutsBottomNavContentElement.is, ShortcutsBottomNavContentElement);
