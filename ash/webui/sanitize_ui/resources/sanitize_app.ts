// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'sanitize-app' is the sanitize app in which based on the url it decides to
 * either show the initial dialog to kickstart sanitize, or show the sanitize
 * done dialog.
 */
import './sanitize_shared.css.js';
import './sanitize_initial.js';
import './sanitize_done.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sanitize_app.html.js';


const SanitizeAppElementBase = I18nMixin(PolymerElement);

export class SanitizeAppElement extends SanitizeAppElementBase {
  static get is() {
    return 'sanitize-app' as const;
  }

  static get template() {
    return getTemplate();
  }
  static get properties(): PolymerElementProperties {
    return {
      // The flow of the sanitize is that by default the initial sanitize page
      // should be shown (showDone = false). Once the user uses sanitize, ash
      // restarts, after the restart, if the sanitizeDone flag is set, the
      // sanitize app should open on the Done page(showDone = True).
      showDone: {type: Boolean, value: loadTimeData.getBoolean('showDone')},
    };
  }

  private showDone: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    [SanitizeAppElement.is]: SanitizeAppElement;
  }
}

customElements.define(SanitizeAppElement.is, SanitizeAppElement);
ColorChangeUpdater.forDocument().start();
