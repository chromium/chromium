// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './auto_tab_groups_in_progress.css.js';
import {getHtml} from './auto_tab_groups_in_progress.html.js';

export interface AutoTabGroupsInProgressElement {
  $: {
    header: HTMLElement,
  };
}

// Loading state for the auto tab groups UI.
export class AutoTabGroupsInProgressElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-in-progress';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'auto-tab-groups-in-progress': AutoTabGroupsInProgressElement;
  }
}

customElements.define(
    AutoTabGroupsInProgressElement.is, AutoTabGroupsInProgressElement);
