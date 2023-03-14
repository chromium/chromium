// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import '../settings_shared.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

import {getTemplate} from './tab_discard_exception_entry.html.js';

export interface TabDiscardExceptionEntry {
  site: string;
  managed: boolean;
}

const TabDiscardExceptionEntryElementBase = BaseMixin(PolymerElement);

export class TabDiscardExceptionEntryElement extends
    TabDiscardExceptionEntryElementBase {
  static get is() {
    return 'tab-discard-exception-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      entry: Object,
      prefs: Object,
    };
  }

  entry: TabDiscardExceptionEntry;

  private onMenuClick_(e: Event) {
    this.fire(
        'menu-click', {target: e.target as HTMLElement, site: this.entry.site});
  }

  private onShowTooltip_() {
    const indicator =
        this.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assert(!!indicator);
    this.fire(
        'show-tooltip', {target: indicator, text: indicator.indicatorTooltip});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-entry': TabDiscardExceptionEntryElement;
  }
}

customElements.define(
    TabDiscardExceptionEntryElement.is, TabDiscardExceptionEntryElement);