// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './shortcut_input_key.html.js';
import {KeyInputState, KeyToIconNameMap} from './shortcut_utils.js';

export const META_KEY = 'meta';
export const LWIN_KEY = 'Meta';

/**
 * @fileoverview
 * 'shortcut-input-key' is a component wrapper for a single input key.
 * Responsible for handling dynamic styling of a single key.
 */

const ShortcutInputKeyElementBase = I18nMixin(PolymerElement);

export class ShortcutInputKeyElement extends ShortcutInputKeyElementBase {
  static get is() {
    return 'shortcut-input-key' as const;
  }

  static get properties(): PolymerElementProperties {
    return {
      key: {
        type: String,
        value: '',
        reflectToAttribute: true,
        observer: ShortcutInputKeyElement.prototype.onKeyChanged,
      },

      keyState: {
        type: String,
        value: KeyInputState.NOT_SELECTED,
        reflectToAttribute: true,
      },

      // If this property is true, the spacing between keys will be narrower
      // than usual.
      narrow: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // If this property is true, keys will be styled with the bolder highlight
      // background.
      highlighted: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // This property is used to apply different styling to keys containing
      // only text and those with icons.
      hasIcon: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // This property is used to apply different icon if the meta key is
      // launcher button.
      hasLauncherButton: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  key: string;
  keyState: KeyInputState;
  narrow: boolean;
  highlighted: boolean;
  hasIcon: boolean;
  hasLauncherButton: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private getIconIdForKey(): string|null {
    // If the key is 'LWIN', then set it as a modifier key.
    if (this.key === LWIN_KEY) {
      this.keyState = KeyInputState.MODIFIER_SELECTED;
    }
    // For 'META_KEY' and 'LWIN' key, return launcher/search icon.
    if (this.key === META_KEY || this.key === LWIN_KEY) {
      return this.hasLauncherButton ? 'shortcut-input-keys:launcher' :
                                      'shortcut-input-keys:search';
    }
    const iconName = KeyToIconNameMap[this.key];
    return iconName ? `shortcut-input-keys:${iconName}` : null;
  }

  /**
   * Returns the GRD string ID for the given key. This function is public and
   * static so that it can be used by the test for this element.
   *
   * @param key The KeyboardEvent.code of a key, e.g. ArrowUp or PrintScreen.
   * @param hasLauncherButton Whether the keyboard has a launcher button or a
   *     search button.
   */
  static getAriaLabelStringId(key: string, hasLauncherButton: boolean): string {
    if (key === META_KEY || key === LWIN_KEY) {
      return hasLauncherButton ? 'iconLabelOpenLauncher' :
                                 'iconLabelOpenSearch';
    }
    return `iconLabel${key}`;  // e.g. iconLabelArrowUp
  }

  private getAriaLabelForIcon(): string {
    const ariaLabelStringId = ShortcutInputKeyElement.getAriaLabelStringId(
        this.key, this.hasLauncherButton);
    assert(
        this.i18nExists(ariaLabelStringId),
        `String ID ${ariaLabelStringId} should exist, but it doesn't.`);

    return this.i18n(ariaLabelStringId);
  }

  private getAriaHidden(): boolean {
    return this.keyState === KeyInputState.NOT_SELECTED;
  }

  private onKeyChanged(): void {
    this.hasIcon = this.key in KeyToIconNameMap;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ShortcutInputKeyElement.is]: ShortcutInputKeyElement;
  }
}

customElements.define(ShortcutInputKeyElement.is, ShortcutInputKeyElement);
