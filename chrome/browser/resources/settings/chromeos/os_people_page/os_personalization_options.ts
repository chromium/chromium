// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'personalization-options' contains several toggles related to
 * personalizations.
 */
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';

import {getTemplate} from './os_personalization_options.html.js';

const OsSettingsPersonalizationOptionsElementBase = PrefsMixin(PolymerElement);

export class OsSettingsPersonalizationOptionsElement extends
    OsSettingsPersonalizationOptionsElementBase {
  static get is() {
    return 'os-settings-personalization-options';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  /**
   * the autocomplete search suggestions CrToggleElement.
   */
  getSearchSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#searchSuggestToggle');
  }

  /**
   * the anonymized URL collection CrToggleElement.
   */
  getUrlCollectionToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#urlCollectionToggle');
  }

  /**
   * the Drive suggestions CrToggleElement.
   */
  getDriveSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#driveSuggestControl');
  }

  // <if expr="_google_chrome">
  private onUseSpellingServiceToggle_(event: Event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if ((event.target as SettingsToggleButtonElement).checked) {
      this.setPrefValue('browser.enable_spellchecking', true);
    }
  }

  private showSpellCheckControlToggle_(): boolean {
    return (
        !!(this.prefs as {spellcheck?: any}).spellcheck &&
        this.getPref<string[]>('spellcheck.dictionaries').value.length > 0);
  }
  // </if><!-- _google_chrome -->
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-personalization-options': OsSettingsPersonalizationOptionsElement;
  }
}

customElements.define(
    OsSettingsPersonalizationOptionsElement.is,
    OsSettingsPersonalizationOptionsElement);
