// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './shared_style.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PaperRippleElement} from 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';

export type FooterInfo = {
  managed: boolean,
  otherFormsOfHistory: boolean,
};

export interface HistorySideBarElement {
  $: {
    'cbd-ripple': PaperRippleElement,
  };
}

export class HistorySideBarElement extends PolymerElement {
  static get is() {
    return 'history-side-bar';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      selectedPage: {
        type: String,
        notify: true,
      },

      guestSession_: Boolean,

      footerInfo: Object,

      historyClustersEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isHistoryClustersEnabled'),
      },

      /**
       * Used to display notices for profile sign-in status and managed status.
       */
      showFooter_: {
        type: Boolean,
        computed: 'computeShowFooter_(' +
            'footerInfo.otherFormsOfHistory, footerInfo.managed)',
      },
    };
  }

  private guestSession_ = loadTimeData.getBoolean('isGuestSession');
  footerInfo: FooterInfo;

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('keydown', e => this.onKeydown_(e));
  }

  private onKeydown_(e: KeyboardEvent) {
    if (e.key === ' ') {
      (e.composedPath()[0] as HTMLElement).click();
    }
  }

  private onSelectorActivate_() {
    this.dispatchEvent(new CustomEvent(
        'history-close-drawer', {bubbles: true, composed: true}));
  }

  /**
   * Relocates the user to the clear browsing data section of the settings page.
   */
  private onClearBrowsingDataTap_(e: Event) {
    const browserService = BrowserService.getInstance();
    browserService.recordAction('InitClearBrowsingData');
    browserService.openClearBrowsingData();
    this.$['cbd-ripple'].upAction();
    e.preventDefault();
  }

  private computeClearBrowsingDataTabIndex_(): string {
    return this.guestSession_ ? '-1' : '';
  }

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   */
  private onItemClick_(e: Event) {
    e.preventDefault();
  }

  private computeShowFooter_(
      includeOtherFormsOfBrowsingHistory: boolean, managed: boolean): boolean {
    return includeOtherFormsOfBrowsingHistory || managed;
  }
}

customElements.define(HistorySideBarElement.is, HistorySideBarElement);
