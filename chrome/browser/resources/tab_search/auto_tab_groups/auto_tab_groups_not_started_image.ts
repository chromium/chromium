// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './auto_tab_groups_not_started_image.css.js';
import {getHtml} from './auto_tab_groups_not_started_image.html.js';

// Themed image for the auto tab groups not started state.
export class AutoTabGroupsNotStartedImageElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-not-started-image';
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
    'auto-tab-groups-not-started-image': AutoTabGroupsNotStartedImageElement;
  }
}

customElements.define(
    AutoTabGroupsNotStartedImageElement.is,
    AutoTabGroupsNotStartedImageElement);
