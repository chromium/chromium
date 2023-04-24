// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the details of a Passpoint
 * subscription.
 */

import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './passpoint_subpage.html.js';

const SettingsPasspointSubpageElementBase = I18nMixin(PolymerElement);

class SettingsPasspointSubpageElement extends
    SettingsPasspointSubpageElementBase {
  static get is() {
    return 'settings-passpoint-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  constructor() {
    super();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPasspointSubpageElement.is]: SettingsPasspointSubpageElement;
  }
}

customElements.define(
    SettingsPasspointSubpageElement.is, SettingsPasspointSubpageElement);
