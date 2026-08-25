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

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import type {OmniboxEverywhereBrowserProxy} from './omnibox_everywhere_browser_proxy.js';
import {OmniboxEverywhereBrowserProxyImpl} from './omnibox_everywhere_browser_proxy.js';
import {getCss} from './omnibox_everywhere_section.css.js';
import {getHtml} from './omnibox_everywhere_section.html.js';

// LINT.IfChange(ShowShortcutsPrefValue)
enum ShowShortcutsPrefValue {
  UNSET = 0,
  DISABLED = 1,
  ENABLED = 2,
}
// LINT.ThenChange(//chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h:ShowShortcutsPrefValue)

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
      isShortcutsShowing_: {type: Boolean},
    };
  }

  protected accessor registeredShortcut_: string = '';
  protected accessor isEnabled_: boolean = false;
  protected accessor isShortcutsShowing_: boolean = false;
  private showShortcutsPrefValue_: number = ShowShortcutsPrefValue.UNSET;
  private ntpShortcutsVisible_: boolean = true;
  private browserProxy_: OmniboxEverywhereBrowserProxy =
      OmniboxEverywhereBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addPrefObserver<boolean>('omnibox_everywhere.enabled', pref => {
      this.isEnabled_ = pref.value;
    });

    this.addPrefObserver<number>('omnibox_everywhere.show_shortcuts', pref => {
      this.showShortcutsPrefValue_ = pref.value;
      this.updateShortcutsShowing_();
    });

    this.addPrefObserver<boolean>('ntp.shortcust_visible', pref => {
      this.ntpShortcutsVisible_ = pref.value;
      this.updateShortcutsShowing_();
    });

    this.browserProxy_.getOmniboxEverywhereShortcut().then(shortcut => {
      this.registeredShortcut_ = shortcut;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.browserProxy_.setOmniboxEverywhereShortcutSuspensionState(false);
  }

  private updateShortcutsShowing_() {
    if (this.showShortcutsPrefValue_ === ShowShortcutsPrefValue.ENABLED) {
      this.isShortcutsShowing_ = true;
    } else if (
        this.showShortcutsPrefValue_ === ShowShortcutsPrefValue.DISABLED) {
      this.isShortcutsShowing_ = false;
    } else {
      this.isShortcutsShowing_ = this.ntpShortcutsVisible_;
    }
  }

  protected async onShowShortcutsToggleChange_(event: Event) {
    const target = event.target as SettingsToggleButtonElement;
    const isChecked = target.checked;
    await PrefService.getInstance().setPrefValue(
        'omnibox_everywhere.show_shortcuts',
        isChecked ? ShowShortcutsPrefValue.ENABLED :
                    ShowShortcutsPrefValue.DISABLED);
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
