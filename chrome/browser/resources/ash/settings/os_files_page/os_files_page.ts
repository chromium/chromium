// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-files-page' is the settings page containing files settings.
 */
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../settings_shared.css.js';
import './files_settings_card.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './os_files_page.html.js';

export class OsSettingsFilesPageElement extends PolymerElement {
  static get is() {
    return 'os-settings-files-page' as const;
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
        value: Section.kFiles,
        readOnly: true,
      },

      shouldShowOneDriveSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOneDriveSettings');
        },
      },

      shouldShowOfficeSettings_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showOfficeSettings');
        },
      },
    };
  }

  prefs: PrefsState|undefined;
  private shouldShowOneDriveSettings_: boolean;
  private shouldShowOfficeSettings_: boolean;
  private section_: Section;
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsFilesPageElement.is]: OsSettingsFilesPageElement;
  }
}

customElements.define(
    OsSettingsFilesPageElement.is, OsSettingsFilesPageElement);
