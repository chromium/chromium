// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './tab_organization_shared_style.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './tab_organization_not_started.html.js';

// Not started state for the tab organization UI.
export class TabOrganizationNotStartedElement extends PolymerElement {
  static get is() {
    return 'tab-organization-not-started';
  }

  static get properties() {
    return {
      showFRE_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showTabOrganizationFRE'),
      },
    };
  }

  private showFRE_: boolean;

  static get template() {
    return getTemplate();
  }

  private getTitle_(): string {
    if (this.showFRE_) {
      return loadTimeData.getString('notStartedTitleFRE');
    } else {
      return loadTimeData.getString('notStartedTitle');
    }
  }

  private getBody_(): string {
    if (this.showFRE_) {
      return loadTimeData.getString('notStartedBodyFRE');
    } else {
      return loadTimeData.getString('notStartedBody');
    }
  }

  private onOrganizeTabsClick_() {
    this.dispatchEvent(new CustomEvent(
        'organize-tabs-click', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-not-started': TabOrganizationNotStartedElement;
  }
}

customElements.define(
    TabOrganizationNotStartedElement.is, TabOrganizationNotStartedElement);
