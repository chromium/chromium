// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date and time
 * settings.
 */

import '../os_settings_page/os_settings_subpage.js';
import '../settings_shared.css.js';
import './date_time_settings_card.js';
import './date_time_types.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './date_time_page.html.js';

export class SettingsDateTimePageElement extends PolymerElement {
  static get is() {
    return 'settings-date-time-page' as const;
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

      section_: {
        type: Number,
        value: Section.kDateAndTime,
        readOnly: true,
      },

      /**
       * This is used to cache the current time zone display name selected from
       * <timezone-selector> via bi-directional binding.
       */
      activeTimeZoneDisplayName: {
        type: String,
        value: loadTimeData.getString('timeZoneName'),
      },
    };
  }

  activeTimeZoneDisplayName: string;
  prefs: PrefsState;
  private section_: Section;
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsDateTimePageElement.is]: SettingsDateTimePageElement;
  }
}

customElements.define(
    SettingsDateTimePageElement.is, SettingsDateTimePageElement);
