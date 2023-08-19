// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './tab_discard_exception_add_input.js';
import './tab_discard_exception_current_sites_list.js';

import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TabDiscardExceptionAddInputElement} from './tab_discard_exception_add_input.js';
import {TabDiscardExceptionCurrentSitesListElement} from './tab_discard_exception_current_sites_list.js';
import {getTemplate} from './tab_discard_exception_tabbed_add_dialog.html.js';

export enum TabDiscardExceptionAddDialogTabs {
  CURRENT_SITES = 0,
  MANUAL = 1,
}

export interface TabDiscardExceptionTabbedAddDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancelButton: CrButtonElement,
    dialog: CrDialogElement,
    input: TabDiscardExceptionAddInputElement,
    list: TabDiscardExceptionCurrentSitesListElement,
    tabs: CrTabsElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionTabbedAddDialogElementBase =
    PrefsMixin(PolymerElement) as
    Constructor<PrefsMixinInterface&PolymerElement>;

export class TabDiscardExceptionTabbedAddDialogElement extends
    TabDiscardExceptionTabbedAddDialogElementBase {
  static get is() {
    return 'tab-discard-exception-tabbed-add-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedTab_: {
        type: Number,
        value: TabDiscardExceptionAddDialogTabs.MANUAL,
      },

      tabNames_: {
        type: Array,
        value: [
          loadTimeData.getString('tabDiscardingExceptionsAddDialogCurrentTabs'),
          loadTimeData.getString('tabDiscardingExceptionsAddDialogManual'),
        ],
      },

      submitDisabledList_: Boolean,
      submitDisabledManual_: Boolean,
    };
  }

  private selectedTab_: TabDiscardExceptionAddDialogTabs;
  private tabNames_: string[];
  private submitDisabledList_: boolean;
  private submitDisabledManual_: boolean;

  private onSitesPopulated_(e: CustomEvent<{length: number}>) {
    if (e.detail.length > 0) {
      this.selectedTab_ = TabDiscardExceptionAddDialogTabs.CURRENT_SITES;
    }
    this.$.dialog.showModal();
  }

  private isAddCurrentSitesTabSelected_() {
    return this.selectedTab_ === TabDiscardExceptionAddDialogTabs.CURRENT_SITES;
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    this.$.dialog.close();
    if (this.isAddCurrentSitesTabSelected_()) {
      this.$.list.submit();
    } else {
      this.$.input.submit();
    }
  }

  private isSubmitDisabled_() {
    if (this.isAddCurrentSitesTabSelected_()) {
      return this.submitDisabledList_;
    }
    return this.submitDisabledManual_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-tabbed-add-dialog':
        TabDiscardExceptionTabbedAddDialogElement;
  }
}

customElements.define(
    TabDiscardExceptionTabbedAddDialogElement.is,
    TabDiscardExceptionTabbedAddDialogElement);
