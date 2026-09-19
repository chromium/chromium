// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../settings_page/settings_subpage.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {DictationBrowserProxyImpl} from './dictation_browser_proxy.js';
import {getCss} from './dictation_page.css.js';
import {getHtml} from './dictation_page.html.js';

export interface SettingsDictationPageElement {
  $: {
    dictationLearnMoreLabel: HTMLAnchorElement,
    shortcutInput: CrShortcutInputElement,
  };
}

const SettingsDictationPageElementBase =
    SettingsViewMixinLit(PrefServiceObserverMixinLit(CrLitElement));

/**
 * Element for the Talk to type (Dictation) settings page.
 * Handles configuration of the dictation hotkey.
 */
export class SettingsDictationPageElement extends
    SettingsDictationPageElementBase {
  static get is() {
    return 'settings-dictation-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      registeredShortcut_: {type: String},
    };
  }

  protected accessor registeredShortcut_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    this.addPrefObserver(
        'browser.voice_typing_hotkey', () => this.onPrefChanged_());
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }

  private async onPrefChanged_() {
    // We don't read the pref directly, as we want a formatted string that the
    // browser produces.
    this.registeredShortcut_ =
        await DictationBrowserProxyImpl.getInstance().getDictationShortcut();
  }

  protected async onShortcutUpdated_(event: CustomEvent<string>) {
    // TODO(b/540531389): Add interaction metrics for hotkey changes.
    const shortcut = event.detail;
    await DictationBrowserProxyImpl.getInstance().setDictationShortcut(
        shortcut);
    this.registeredShortcut_ =
        await DictationBrowserProxyImpl.getInstance().getDictationShortcut();
  }
}

export type DictationPageElement = SettingsDictationPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'settings-dictation-page': SettingsDictationPageElement;
  }
}

customElements.define(
    SettingsDictationPageElement.is, SettingsDictationPageElement);
