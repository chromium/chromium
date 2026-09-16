// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../settings_shared.css.js';
import '../settings_page/settings_subpage.js';

import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {DictationBrowserProxyImpl} from './dictation_browser_proxy.js';
import {getTemplate} from './dictation_page.html.js';

export interface SettingsDictationPageElement {
  $: {
    dictationLearnMoreLabel: HTMLAnchorElement,
    shortcutInput: CrShortcutInputElement,
  };
}

const SettingsDictationPageElementBase =
    SettingsViewMixin(PrefServiceObserverMixin(PolymerElement));

/**
 * Polymer element for the Talk to type (Dictation) settings page.
 * Handles configuration of the dictation hotkey.
 */
export class SettingsDictationPageElement extends
    SettingsDictationPageElementBase {
  static get is() {
    return 'settings-dictation-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      registeredShortcut_: {
        type: String,
        value: '',
      },
    };
  }

  declare private registeredShortcut_: string;

  override connectedCallback() {
    super.connectedCallback();
    this.addPrefObserver(
        'browser.voice_typing_hotkey', () => this.onPrefChanged_());
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }

  private async onPrefChanged_() {
    // We don't read the pref directly, as we want a formatted string that the
    // browser produces.
    this.registeredShortcut_ =
        await DictationBrowserProxyImpl.getInstance().getDictationShortcut();
  }

  private async onShortcutUpdated_(event: CustomEvent<string>) {
    // TODO(b/540531389): Add interaction metrics for hotkey changes.
    const shortcut = event.detail;
    await DictationBrowserProxyImpl.getInstance().setDictationShortcut(
        shortcut);
    this.registeredShortcut_ =
        await DictationBrowserProxyImpl.getInstance().getDictationShortcut();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-dictation-page': SettingsDictationPageElement;
  }
}

customElements.define(
    SettingsDictationPageElement.is, SettingsDictationPageElement);
