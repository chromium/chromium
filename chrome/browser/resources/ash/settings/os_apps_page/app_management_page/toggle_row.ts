// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_management_cros_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import '//resources/ash/common/cr_elements/icons.html.js';

import {CrToggleElement} from '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './toggle_row.html.js';

export interface AppManagementToggleRowElement {
  $: {toggle: CrToggleElement};
}

export class AppManagementToggleRowElement extends PolymerElement {
  static get is() {
    return 'app-management-toggle-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      icon: String,
      label: String,
      managed: {type: Boolean, value: false, reflectToAttribute: true},
      disabled: {type: Boolean, value: false, reflectToAttribute: true},
      value: {type: Boolean, value: false, reflectToAttribute: true},
      description: String,
    };
  }

  icon: string;
  label: string;
  managed: boolean;
  disabled: boolean;
  value: boolean;
  description: string;

  override ready(): void {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  isChecked(): boolean {
    return this.$.toggle.checked;
  }

  setToggle(value: boolean): void {
    this.$.toggle.checked = value;
  }

  private isDisabled_(disabled: boolean, managed: boolean): boolean {
    return disabled || managed;
  }

  private onClick_(event: Event): void {
    event.stopPropagation();
    this.$.toggle.click();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-toggle-row': AppManagementToggleRowElement;
  }
}

customElements.define(
    AppManagementToggleRowElement.is, AppManagementToggleRowElement);
