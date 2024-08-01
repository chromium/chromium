// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './exception_add_input.js';
import './exception_current_sites_list.js';

import type {PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {NONE_SELECTED} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import type {ExceptionAddInputElement} from './exception_add_input.js';
import type {ExceptionCurrentSitesListElement} from './exception_current_sites_list.js';
import {getTemplate} from './exception_tabbed_add_dialog.html.js';

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

type Constructor<T> = new (...args: any[]) => T;
const ExceptionTabbedAddDialogElementBase =
    PrefsMixin(PolymerElement) as
    Constructor<PrefsMixinInterface&PolymerElement>;

export class ExceptionTabbedAddDialogElement extends
    ExceptionTabbedAddDialogElementBase {
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
        value: NONE_SELECTED,
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

  private selectedTab_: ExceptionAddDialogTabs;
  private tabNames_: string[];
  private submitDisabledList_: boolean;
  private submitDisabledManual_: boolean;

  private onSelectedTabChanged_() {
    // Asynchronously notify the list that its visibility has changed. This is
    // necessary because the list has an iron-list child that needs to be
    // manually notified of visibility changes that are triggered by any element
    // that does not implement iron-resizable-behavior.
    setTimeout(() => this.$.list.notifyResize(), 0);
  }

  private onSitesPopulated_(e: CustomEvent<{length: number}>) {
    if (e.detail.length > 0) {
      this.selectedTab_ = ExceptionAddDialogTabs.CURRENT_SITES;
    } else if (this.selectedTab_ === NONE_SELECTED) {
      this.selectedTab_ = ExceptionAddDialogTabs.MANUAL;
    }
    this.$.dialog.showModal();
  }

  private isAddCurrentSitesTabSelected_() {
    return this.selectedTab_ === ExceptionAddDialogTabs.CURRENT_SITES;
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
        ExceptionTabbedAddDialogElement;
  }
}

customElements.define(
    ExceptionTabbedAddDialogElement.is,
    ExceptionTabbedAddDialogElement);
