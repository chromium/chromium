// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_results_actions.html.js';

export class TabOrganizationResultsActionsElement extends PolymerElement {
  static get is() {
    return 'tab-organization-results-actions';
  }

  static get properties() {
    return {
      isLastOrganization: Boolean,
      multipleOrganizations: Boolean,

      showRefresh_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('tabOrganizationRefreshButtonEnabled'),
      },
    };
  }

  isLastOrganization: boolean;
  multipleOrganizations: boolean;

  private showRefresh_: boolean;

  static get template() {
    return getTemplate();
  }

  private getRefreshButtonText_(): string {
    return (this.isLastOrganization || this.multipleOrganizations) ?
        loadTimeData.getString('rejectFinalSuggestion') :
        loadTimeData.getString('rejectSuggestion');
  }

  private getCreateButtonText_(): string {
    return this.multipleOrganizations ? loadTimeData.getString('createGroups') :
                                        loadTimeData.getString('createGroup');
  }

  private onRejectGroupClick_() {
    this.dispatchEvent(new CustomEvent('reject-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private onCreateGroupClick_() {
    this.dispatchEvent(new CustomEvent('create-group-click', {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-results-actions': TabOrganizationResultsActionsElement;
  }
}

customElements.define(
    TabOrganizationResultsActionsElement.is,
    TabOrganizationResultsActionsElement);
