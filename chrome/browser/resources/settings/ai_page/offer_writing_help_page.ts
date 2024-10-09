// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './offer_writing_help_page.html.js';

export const COMPOSE_PROACTIVE_NUDGE_PREF = 'compose.proactive_nudge_enabled';
export const COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF =
    'compose.proactive_nudge_disabled_sites_with_time';

const SettingsOfferWritingHelpPageElementBase =
    I18nMixin(ListPropertyUpdateMixin(PrefsMixin(PolymerElement)));

export class SettingsOfferWritingHelpPageElement extends
    SettingsOfferWritingHelpPageElementBase {
  static get is() {
    return 'settings-offer-writing-help-page';
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
      siteList_: {
        type: Array,
        value: [],
      },
      enableAiSettingsPageRefresh_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableAiSettingsPageRefresh'),
      },
      enableComposeProactiveNudge_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableComposeProactiveNudge'),
      },
    };
  }

  static get observers() {
    return [`onPrefsChanged_(
        prefs.${COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF}.value.*)`];
  }

  private siteList_: string[];
  private enableAiSettingsPageRefresh_: boolean;
  private enableComposeProactiveNudge_: boolean;

  private hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  private onDeleteClick_(e: DomRepeatEvent<string>) {
    this.deletePrefDictEntry(
        COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF, e.model.item);
  }

  private onPrefsChanged_() {
    const prefDict = this.getPref<Record<string, number>>(
                             COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF)
                         .value;
    const newSites = Object.keys(prefDict);

    this.updateList('siteList_', (entry: string) => entry, newSites);
  }

  private getProactiveNudgeToggleHrCssClass_(): string {
    return this.enableAiSettingsPageRefresh_ ? 'hr' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-offer-writing-help-page': SettingsOfferWritingHelpPageElement;
  }
}

customElements.define(
    SettingsOfferWritingHelpPageElement.is,
    SettingsOfferWritingHelpPageElement);
