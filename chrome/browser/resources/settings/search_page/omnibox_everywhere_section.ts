// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-omnibox-everywhere-section' is the settings section containing
 * controls for Omnibox Everywhere (Search in Chrome).
 */
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_section.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereBrowserProxy} from './omnibox_everywhere_browser_proxy.js';
import {OmniboxEverywhereBrowserProxyImpl} from './omnibox_everywhere_browser_proxy.js';
import {getCss} from './omnibox_everywhere_section.css.js';
import {getHtml} from './omnibox_everywhere_section.html.js';

export interface SettingsOmniboxEverywhereSectionElement {
  $: {
    shortcutInput: CrShortcutInputElement,
  };
}

const SettingsOmniboxEverywhereSectionElementBase =
    PrefServiceObserverMixinLit(I18nMixinLit(CrLitElement));

export class SettingsOmniboxEverywhereSectionElement extends
    SettingsOmniboxEverywhereSectionElementBase {
  static get is() {
    return 'settings-omnibox-everywhere-section';
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
      isEnabled_: {type: Boolean},
    };
  }

  protected accessor registeredShortcut_: string = '';
  protected accessor isEnabled_: boolean = false;
  private browserProxy_: OmniboxEverywhereBrowserProxy =
      OmniboxEverywhereBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addPrefObserver<boolean>('omnibox_everywhere.enabled', pref => {
      this.isEnabled_ = pref.value;
    });

    this.browserProxy_.getOmniboxEverywhereShortcut().then(shortcut => {
      this.registeredShortcut_ = shortcut;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.browserProxy_.setOmniboxEverywhereShortcutSuspensionState(false);
  }

  protected async onShortcutUpdated_(event: CustomEvent<string>) {
    const shortcut = event.detail;
    await this.browserProxy_.setOmniboxEverywhereShortcut(shortcut);
    this.registeredShortcut_ =
        await this.browserProxy_.getOmniboxEverywhereShortcut();
  }

  protected onInputCaptureChange_(event: CustomEvent<boolean>) {
    this.browserProxy_.setOmniboxEverywhereShortcutSuspensionState(
        event.detail);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-omnibox-everywhere-section':
        SettingsOmniboxEverywhereSectionElement;
  }
}

customElements.define(
    SettingsOmniboxEverywhereSectionElement.is,
    SettingsOmniboxEverywhereSectionElement);
