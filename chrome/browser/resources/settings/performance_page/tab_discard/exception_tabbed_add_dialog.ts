// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './exception_add_input.js';
import './exception_current_sites_list.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {NONE_SELECTED} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';

import type {ExceptionAddInputElement} from './exception_add_input.js';
import type {ExceptionCurrentSitesListElement} from './exception_current_sites_list.js';
import {getCss} from './exception_tabbed_add_dialog.css.js';
import {getHtml} from './exception_tabbed_add_dialog.html.js';

export enum ExceptionAddDialogTabs {
  CURRENT_SITES = 0,
  MANUAL = 1,
}

export interface ExceptionTabbedAddDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: ExceptionAddInputElement,
    list: ExceptionCurrentSitesListElement,
    tabs: CrTabsElement,
  };
}

export class ExceptionTabbedAddDialogElement extends CrLitElement {
  static get is() {
    return 'tab-discard-exception-tabbed-add-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedTab_: {type: Number},
      tabNames_: {type: Array},
      submitDisabledList_: {type: Boolean},
      submitDisabledManual_: {type: Boolean},
    };
  }

  protected accessor selectedTab_: ExceptionAddDialogTabs = NONE_SELECTED;
  protected accessor tabNames_: string[] = [
    loadTimeData.getString('tabDiscardingExceptionsAddDialogCurrentTabs'),
    loadTimeData.getString('tabDiscardingExceptionsAddDialogManual'),
  ];
  protected accessor submitDisabledList_: boolean = true;
  protected accessor submitDisabledManual_: boolean = true;

  protected onTabsSelectedChanged_(
      e: CustomEvent<{value: ExceptionAddDialogTabs}>) {
    this.selectedTab_ = e.detail.value;
  }

  protected onListSubmitDisabledChanged_(e: CustomEvent<{value: boolean}>) {
    this.submitDisabledList_ = e.detail.value;
  }

  protected onInputSubmitDisabledChanged_(e: CustomEvent<{value: boolean}>) {
    this.submitDisabledManual_ = e.detail.value;
  }

  protected onSitesPopulated_(e: CustomEvent<{length: number}>) {
    if (e.detail.length > 0) {
      this.selectedTab_ = ExceptionAddDialogTabs.CURRENT_SITES;
    } else if (this.selectedTab_ === NONE_SELECTED) {
      this.selectedTab_ = ExceptionAddDialogTabs.MANUAL;
    }
    this.$.dialog.showModal();
  }

  protected isAddCurrentSitesTabSelected_(): boolean {
    return this.selectedTab_ === ExceptionAddDialogTabs.CURRENT_SITES;
  }

  protected onCancelClick_() {
    this.$.dialog.cancel();
  }

  protected onSubmitClick_() {
    this.$.dialog.close();
    if (this.isAddCurrentSitesTabSelected_()) {
      this.$.list.submit();
    } else {
      this.$.input.submit();
    }
  }

  protected isSubmitDisabled_(): boolean {
    if (this.isAddCurrentSitesTabSelected_()) {
      return this.submitDisabledList_;
    }
    return this.submitDisabledManual_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-tabbed-add-dialog':
        ExceptionTabbedAddDialogElement;
  }
}

customElements.define(
    ExceptionTabbedAddDialogElement.is,
    ExceptionTabbedAddDialogElement);
