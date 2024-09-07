// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-prediction-improvements-section' is
 * the section containing configuration options for prediction improvements.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './autofill_prediction_improvements_section.html.js';
import type {UserAnnotationsManagerProxy} from './user_annotations_manager_proxy.js';
import {UserAnnotationsManagerProxyImpl} from './user_annotations_manager_proxy.js';

type UserAnnotationsEntry = chrome.autofillPrivate.UserAnnotationsEntry;



export interface SettingsAutofillPredictionImprovementsSectionElement {
  $: {
    prefToggle: HTMLElement,
  };
}

export class SettingsAutofillPredictionImprovementsSectionElement extends
    PolymerElement {
  static get is() {
    return 'settings-autofill-prediction-improvements-section';
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

  prefs: {[key: string]: any};
  private userAnnotationsEntries_: UserAnnotationsEntry[];
  private userAnnotationsManager_: UserAnnotationsManagerProxy =
      UserAnnotationsManagerProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.userAnnotationsManager_.getEntries().then(
        (entries: UserAnnotationsEntry[]) => {
          this.userAnnotationsEntries_ = entries;
        });
  }

  private onToggleSubLabelLinkClick_(): void {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('addressesAndPaymentMethodsLearnMoreURL'));
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
