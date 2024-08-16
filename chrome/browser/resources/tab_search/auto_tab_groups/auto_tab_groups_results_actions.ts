// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'auto-tab-groups-results-actions' is a row with actions that
 * can be taken on an auto tab groups suggestion. It is agnostic as to what
 * that suggestion is, and can be used to suggest one or multiple groups.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './auto_tab_groups_results_actions.css.js';
import {getHtml} from './auto_tab_groups_results_actions.html.js';

export class AutoTabGroupsResultsActionsElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-results-actions';
  }

  static override get properties() {
    return {
      multipleOrganizations: {type: Boolean},
      showClear: {type: Boolean},
    };
  }

  multipleOrganizations: boolean = false;
  showClear: boolean = false;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected getCreateButtonText_(): string {
    return this.multipleOrganizations ? loadTimeData.getString('createGroups') :
                                        loadTimeData.getString('createGroup');
  }

  protected onClearClick_() {
    this.fire('reject-all-groups-click');
  }

  protected onCreateGroupClick_() {
    this.fire('create-group-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'auto-tab-groups-results-actions': AutoTabGroupsResultsActionsElement;
  }
}

customElements.define(
    AutoTabGroupsResultsActionsElement.is, AutoTabGroupsResultsActionsElement);
