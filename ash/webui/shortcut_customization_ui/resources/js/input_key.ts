// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './input_key.html.js';
import {keyToIconNameMap, LWIN_KEY, META_KEY} from './shortcut_utils.js';

/**
 * Refers to the state of an 'input-key' item.
 */
export enum KeyInputState {
  NOT_SELECTED = 'not-selected',
  MODIFIER_SELECTED = 'modifier-selected',
  ALPHANUMERIC_SELECTED = 'alpha-numeric-selected',
}

/**
 * @fileoverview
 * 'input-key' is a component wrapper for a single input key. Responsible for
 * handling dynamic styling of a single key.
 */

const InputKeyElementBase = I18nMixin(PolymerElement);

export class InputKeyElement extends InputKeyElementBase {
  static get is(): string {
    return 'input-key';
  }

  static get properties(): PolymerElementProperties {
    return {
      key: {
        type: String,
        value: '',
        reflectToAttribute: true,
        observer: InputKeyElement.prototype.onKeyChanged,
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
    };
  }

  key: string;
  keyState: KeyInputState;
  narrow: boolean;
  highlighted: boolean;
  hasIcon: boolean;
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

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
      const hasLauncherButton = this.lookupManager.getHasLauncherButton();
      return hasLauncherButton ? 'shortcut-customization-keys:launcher' :
                                 'shortcut-customization-keys:search';
    }
    const iconName = keyToIconNameMap[this.key];
    return iconName ? `shortcut-customization-keys:${iconName}` : null;
  }

  private onKeyChanged(): void {
    this.hasIcon = this.key in keyToIconNameMap;
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
    const hasLauncherButton = this.lookupManager.getHasLauncherButton();
    const ariaLabelStringId =
        InputKeyElement.getAriaLabelStringId(this.key, hasLauncherButton);
    assert(
        this.i18nExists(ariaLabelStringId),
        `String ID ${ariaLabelStringId} should exist, but it doesn't.`);

    return this.i18n(ariaLabelStringId);
  }

  // Prevent announcing input keys when in editing mode.
  private getAriaHidden(): boolean {
    return this.keyState === KeyInputState.NOT_SELECTED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'input-key': InputKeyElement;
  }
}

customElements.define(InputKeyElement.is, InputKeyElement);
