// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-and-power-settings-card' is the card element containing storage and
 * power settings.
 */

import '../controls/settings_toggle_button.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {shouldShowMultitasking} from '../common/load_time_booleans.js';
import {PrefsState} from '../common/types.js';

import {getTemplate} from './multitasking_settings_card.html.js';


const MultitaskingSettingsCardElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class MultitaskingSettingsCardElement extends
    MultitaskingSettingsCardElementBase {
  static get is() {
    return 'multitasking-settings-card' as const;
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
      shouldShowMultitasking_: {
        type: Boolean,
        value() {
          return shouldShowMultitasking();
        },
        readOnly: true,
      },
    };
  }

  prefs: PrefsState;

  // The following strings are only defined when the OsSettingsRevampWayfinding
  // feature flag is enabled. Avoid using $i18n{} templating in HTML to avoid
  // crashes when the feature is disabled.
  private getHeaderText_(): string {
    return this.i18n('multitaskingSettingsCardTitle');
  }
  private getLabelText_(): string {
    return this.i18n('snapWindowLabel');
  }
  private getDescriptionText_(): string {
    return this.i18n('snapWindowDescription');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MultitaskingSettingsCardElement.is]: MultitaskingSettingsCardElement;
  }
}

customElements.define(
    MultitaskingSettingsCardElement.is, MultitaskingSettingsCardElement);
