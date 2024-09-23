// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './auto_tab_groups_new_badge.css.js';
import {getHtml} from './auto_tab_groups_new_badge.html.js';

// New badge divider for the auto tab groups UI.
export class AutoTabGroupsNewBadgeElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-new-badge';
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
    'auto-tab-groups-new-badge': AutoTabGroupsNewBadgeElement;
  }
}

customElements.define(
    AutoTabGroupsNewBadgeElement.is, AutoTabGroupsNewBadgeElement);
