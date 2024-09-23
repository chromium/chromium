// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './strings.m.js';
import './tangible_sync_style_shared.css.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowsingDataHandling} from './managed_user_profile_notice_browser_proxy.js';
import {getCss} from './managed_user_profile_notice_data_handling.css.js';
import {getHtml} from './managed_user_profile_notice_data_handling.html.js';

export class ManagedUserProfileNoticeDataHandlingElement extends CrLitElement {
  static get is() {
    return 'managed-user-profile-notice-data-handling';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      title: {type: String},
      selectedDataHandling: {type: String, notify: true},
    };
  }

  override title: string;
  selectedDataHandling: BrowsingDataHandling;

  protected onSelectedRadioOptionChanged_(
      e: CustomEvent<{value: BrowsingDataHandling}>) {
    this.selectedDataHandling = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-data-handling':
        ManagedUserProfileNoticeDataHandlingElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeDataHandlingElement.is,
    ManagedUserProfileNoticeDataHandlingElement);
