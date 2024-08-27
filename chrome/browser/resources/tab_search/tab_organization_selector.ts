// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './auto_tab_groups/auto_tab_groups_page.js';
import './declutter/declutter_page.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './tab_organization_selector.css.js';
import {getHtml} from './tab_organization_selector.html.js';

export enum OrganizationFeature {
  NONE = 0,
  AUTO_TAB_GROUPS = 1,
  DECLUTTER = 2,
}

export class TabOrganizationSelectorElement extends CrLitElement {
  static get is() {
    return 'tab-organization-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedState_: {type: Number},
    };
  }

  protected selectedState_: OrganizationFeature = OrganizationFeature.NONE;

  protected onAutoTabGroupsClick_(): void {
    this.selectedState_ = OrganizationFeature.AUTO_TAB_GROUPS;
    const autoTabGroupsPage =
        this.shadowRoot!.querySelector('auto-tab-groups-page')!;
    autoTabGroupsPage.classList.toggle('changed-state', false);
  }

  protected onDeclutterClick_(): void {
    this.selectedState_ = OrganizationFeature.DECLUTTER;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-selector': TabOrganizationSelectorElement;
  }
}

customElements.define(
    TabOrganizationSelectorElement.is, TabOrganizationSelectorElement);
