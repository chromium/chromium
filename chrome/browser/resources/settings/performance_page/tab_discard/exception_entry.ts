// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './exception_entry.css.js';
import {getHtml} from './exception_entry.html.js';
import {TAB_DISCARD_EXCEPTIONS_MANAGED_PREF} from './exception_validation_mixin.js';

export interface ExceptionEntry {
  site: string;
  managed: boolean;
}

const ExceptionEntryElementBase = PrefServiceObserverMixinLit(CrLitElement);

export class ExceptionEntryElement extends ExceptionEntryElementBase {
  static get is() {
    return 'tab-discard-exception-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entry: {type: Object},
      exceptionsManagedPref_: {type: Object},
    };
  }

  accessor entry: ExceptionEntry;
  protected accessor exceptionsManagedPref_: chrome.settingsPrivate.PrefObject|
      undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPref(
        TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, 'exceptionsManagedPref_');
  }

  protected onMenuClick_(e: Event) {
    this.fire(
        'menu-click', {target: e.target as HTMLElement, site: this.entry.site});
  }

  protected onShowTooltipMouseenter_() {
    this.showTooltip_();
  }

  protected onShowTooltipFocus_() {
    this.showTooltip_();
  }

  private showTooltip_() {
    const indicator = this.shadowRoot.querySelector('cr-policy-pref-indicator');
    assert(indicator);
    this.fire(
        'show-tooltip', {target: indicator, text: indicator.indicatorTooltip});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-entry': ExceptionEntryElement;
  }
}

customElements.define(
    ExceptionEntryElement.is, ExceptionEntryElement);
