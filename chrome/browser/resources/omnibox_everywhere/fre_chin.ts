// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './fre_chin.css.js';
import {getHtml} from './fre_chin.html.js';

export enum FreChinMode {
  SHORTCUT_SETUP = 'shortcut-setup',
  SHORTCUT_REMINDER = 'shortcut-reminder',
}

export interface ShowHotkeyDropdownDetail {
  x: number;
  y: number;
  width: number;
  height: number;
}

const OmniboxEverywhereFreChinElementBase = I18nMixinLit(CrLitElement);

export class OmniboxEverywhereFreChinElement extends
    OmniboxEverywhereFreChinElementBase {
  static get is() {
    return 'fre-chin';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      mode: {type: String},
      hotkeyTokens: {type: Array},
      dropdownOpen: {type: Boolean},
    };
  }

  accessor mode: FreChinMode = FreChinMode.SHORTCUT_SETUP;
  accessor hotkeyTokens: string[] = [];
  accessor dropdownOpen: boolean = false;

  protected isSetupMode_(): boolean {
    return this.mode === FreChinMode.SHORTCUT_SETUP;
  }

  protected hasHotkeyTokens_(): boolean {
    return Array.isArray(this.hotkeyTokens) && this.hotkeyTokens.length > 0;
  }

  protected getDropdownAriaLabel_(): string {
    const baseLabel = this.i18n('loomniboxFreSelectKeyboardShortcut');
    if (this.hasHotkeyTokens_()) {
      return `${baseLabel}: ${this.hotkeyTokens.join(' ')}`;
    }
    return baseLabel;
  }

  protected getToSearchLabelHtml_(): TrustedHTML {
    return this.i18nAdvanced('loomniboxFreToSearchOrCustomize', {
      attrs: ['class'],
    });
  }

  protected getChangeShortcutLabelHtml_(): TrustedHTML {
    return this.i18nAdvanced('loomniboxFreChangeShortcutIn', {
      attrs: ['class'],
    });
  }

  protected onDropdownClick_(e: MouseEvent) {
    e.stopPropagation();
    const trigger = e.currentTarget as HTMLElement;
    const rect = trigger.getBoundingClientRect();
    const detail: ShowHotkeyDropdownDetail = {
      x: Math.round(rect.left),
      y: Math.round(rect.top),
      width: Math.round(rect.width),
      height: Math.round(rect.height),
    };
    this.fire('show-hotkey-dropdown', detail);
  }

  protected onLabelClick_(e: MouseEvent) {
    const target = e.target as HTMLElement | null;
    if (target?.closest('.settings-link') || target?.closest('a')) {
      this.onSettingsClick_(e);
    }
  }

  protected onSettingsClick_(e: Event) {
    e.preventDefault();
    this.fire('open-settings');
  }

  protected onCloseClick_() {
    this.fire('close');
  }
}

export type FreChinElement = OmniboxEverywhereFreChinElement;

declare global {
  interface HTMLElementTagNameMap {
    'fre-chin': OmniboxEverywhereFreChinElement;
  }
}

customElements.define(
    OmniboxEverywhereFreChinElement.is, OmniboxEverywhereFreChinElement);
