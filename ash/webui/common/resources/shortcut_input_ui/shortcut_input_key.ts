// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
// <if expr="_google_chrome" >
import 'chrome://resources/ash/common/internal/ash_internal_icons.html.js';
// </if>

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './shortcut_input_key.html.js';
import {KeyInputState, KeyToIconNameMap, MetaKey} from './shortcut_utils.js';
// <if expr="_google_chrome" >
import {KeyToInternalIconNameMap, KeyToInternalIconNameRefreshOnlyMap} from './shortcut_utils.js';
// </if>

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
      metaKey: {
        type: Object,
        reflectToAttribute: true,
      },
    };
  }

  key: string;
  keyState: KeyInputState;
  narrow: boolean;
  highlighted: boolean;
  hasIcon: boolean;
  metaKey: MetaKey = MetaKey.kSearch;

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
      switch (this.metaKey) {
        case MetaKey.kLauncherRefresh:
          return 'ash-internal:launcher-refresh';
        case MetaKey.kSearch:
          return 'shortcut-input-keys:search';
        case MetaKey.kLauncher:
        default:
          return 'shortcut-input-keys:launcher';
      }
    }

    // <if expr="_google_chrome" >
    const internalIconName = KeyToInternalIconNameMap[this.key];
    if (internalIconName) {
      return `ash-internal:${internalIconName}`;
    }

    const internalRefreshIconName =
        KeyToInternalIconNameRefreshOnlyMap[this.key];
    if (internalRefreshIconName && this.metaKey === MetaKey.kLauncherRefresh) {
      return `ash-internal:${internalRefreshIconName}`;
    }
    // </if>

    const iconName = KeyToIconNameMap[this.key];
    if (iconName) {
      return `shortcut-input-keys:${iconName}`;
    }

    return null;
  }

  /**
   * Returns the GRD string ID for the given key. This function is public and
   * static so that it can be used by the test for this element.
   *
   * @param key The KeyboardEvent.code of a key, e.g. ArrowUp or PrintScreen.
   * @param metaKey The keyboard' meta key to display in the UI,
   *    e.g. Search or Launcher.
   */
  static getAriaLabelStringId(key: string, metaKey: MetaKey): string {
    if (key === META_KEY || key === LWIN_KEY) {
      switch (metaKey) {
        case MetaKey.kLauncherRefresh:
          // TODO(b/338134189): Replace it with updated string id when
          // finalized.
          return 'iconLabelOpenLauncher';
        case MetaKey.kSearch:
          return 'iconLabelOpenSearch';
        case MetaKey.kLauncher:
        default:
          return 'iconLabelOpenLauncher';
      }
    }
    return `iconLabel${key}`;  // e.g. iconLabelArrowUp
  }

  private getAriaLabelForIcon(): string {
    const ariaLabelStringId =
        ShortcutInputKeyElement.getAriaLabelStringId(this.key, this.metaKey);
    assert(
        this.i18nExists(ariaLabelStringId),
        `String ID ${ariaLabelStringId} should exist, but it doesn't.`);

    return this.i18n(ariaLabelStringId);
  }

  private onKeyChanged(): void {
    if (this.key in KeyToIconNameMap) {
      this.hasIcon = true;
      return;
    }

    // <if expr="_google_chrome" >
    if (this.key in KeyToInternalIconNameMap) {
      this.hasIcon = true;
      return;
    }
    // </if>

    this.hasIcon = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ShortcutInputKeyElement.is]: ShortcutInputKeyElement;
  }
}

customElements.define(ShortcutInputKeyElement.is, ShortcutInputKeyElement);
