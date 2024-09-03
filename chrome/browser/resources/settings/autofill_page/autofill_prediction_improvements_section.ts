// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-prediction-improvements-section' is
 * the section containing configuration options for prediction improvements.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './autofill_prediction_improvements_section.html.js';


export class SettingsAutofillPredictionImprovementsSectionElement extends
    PolymerElement {
  static get is() {
    return 'settings-autofill-prediction-improvements-section';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-autofill-prediction-improvements-section':
        SettingsAutofillPredictionImprovementsSectionElement;
  }
}

customElements.define(
    SettingsAutofillPredictionImprovementsSectionElement.is,
    SettingsAutofillPredictionImprovementsSectionElement);
