// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './auto_tab_groups_failure.css.js';
import {getHtml} from './auto_tab_groups_failure.html.js';
import {TabOrganizationError} from '../tab_search.mojom-webui.js';

export interface AutoTabGroupsFailureElement {
  $: {
    header: HTMLElement,
  };
}

// Failure state for the auto tab groups UI.
export class AutoTabGroupsFailureElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-failure';
  }

  static override get properties() {
    return {
      error: {type: Number},
      showFre: {type: Boolean},
    };
  }

  error: TabOrganizationError = TabOrganizationError.kNone;
  showFre: boolean = false;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected getBody_(): string {
    switch (this.error) {
      case TabOrganizationError.kGrouping:
        return loadTimeData.getString('failureBodyGrouping');
      case TabOrganizationError.kGeneric:
        return loadTimeData.getString('failureBodyGeneric');
      default:
        return '';
    }
  }

  protected onCheckNow_(e: CustomEvent<{event: Event}>) {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.fire('check-now');
  }

  protected onTipClick_() {
    this.fire('tip-click');
  }

  protected onTipKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.onTipClick_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'auto-tab-groups-failure': AutoTabGroupsFailureElement;
  }
}

customElements.define(
    AutoTabGroupsFailureElement.is, AutoTabGroupsFailureElement);
