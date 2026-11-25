// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-geic-assistant-switcher' displays the switcher section shared by
 * the GEiC and Glic subpages for switching between Gemini Enterprise and
 * consumer Gemini when offered by a device-level policy.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../icons.html.js';
// <if expr="not is_chromeos">
import '../relaunch_confirmation_dialog.js';

// </if>

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {GlicAssistantSelection} from '../ai_page/constants.js';
import {GlicBrowserProxyImpl} from '../glic_page/glic_browser_proxy.js';
import {RelaunchMixinLit, RestartType} from '../relaunch_mixin_lit.js';

import {getCss} from './geic_assistant_switcher.css.js';
import {getHtml} from './geic_assistant_switcher.html.js';

export interface SettingsGeicAssistantSwitcherElement {
  $: {
    consider: HTMLElement,
    label: HTMLElement,
    row: HTMLElement,
    section: HTMLElement,
    sublabel: HTMLElement,
    switchButton: CrButtonElement,
  };
}

const SettingsGeicAssistantSwitcherElementBase = RelaunchMixinLit(CrLitElement);

export class SettingsGeicAssistantSwitcherElement extends
    SettingsGeicAssistantSwitcherElementBase {
  static get is() {
    return 'settings-geic-assistant-switcher';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      targetSelection: {type: Number},
      buttonDisabled_: {type: Boolean},
    };
  }

  accessor targetSelection: GlicAssistantSelection =
      GlicAssistantSelection.AUTO;
  protected accessor buttonDisabled_: boolean = false;

  override onRelaunchDialogClose(event: Event) {
    this.buttonDisabled_ = false;
    super.onRelaunchDialogClose(event);
  }

  protected isEnterpriseTarget_(): boolean {
    return this.targetSelection === GlicAssistantSelection.ENTERPRISE;
  }

  protected getFirstConsiderIcon_(): string {
    return this.isEnterpriseTarget_() ? 'settings20:security' :
                                        'settings20:draft-spark';
  }

  protected onSwitchButtonClick_() {
    assert(
        this.targetSelection === GlicAssistantSelection.CONSUMER ||
        this.targetSelection === GlicAssistantSelection.ENTERPRISE);
    this.buttonDisabled_ = true;
    this.performRestart(
        RestartType.RESTART, /*alwaysShowDialog=*/ false, () => {
          // The restart is going ahead, so keep the button disabled.
          this.buttonDisabled_ = true;
          GlicBrowserProxyImpl.getInstance().switchAssistantSelection(
              this.targetSelection);
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-geic-assistant-switcher': SettingsGeicAssistantSwitcherElement;
  }
}

customElements.define(
    SettingsGeicAssistantSwitcherElement.is,
    SettingsGeicAssistantSwitcherElement);
