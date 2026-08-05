// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './install_dev_proxy_tab.js';
import './install_local_bundle_tab.js';
import './install_update_manifest_tab.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './install_dialog.css.js';
import {getHtml} from './install_dialog.html.js';
import type {IwaDevInstallTabElement} from './install_tab.js';

export enum TabIndex {
  DEV_PROXY = 0,
  LOCAL_BUNDLE = 1,
  UPDATE_MANIFEST = 2,
}

export interface IwaDevInstallDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class IwaDevInstallDialogElement extends CrLitElement {
  static get is() {
    return 'iwa-dev-install-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedTab_: {type: Number, state: true},
      isInstalling_: {type: Boolean, state: true},
      installationError_: {type: String, state: true},
      isCurrentTabValid_: {type: Boolean, state: true},
      isOpened_: {type: Boolean, state: true},
    };
  }

  protected accessor selectedTab_: TabIndex = TabIndex.DEV_PROXY;
  protected accessor isInstalling_: boolean = false;
  protected accessor installationError_: string = '';
  protected accessor isCurrentTabValid_: boolean = false;
  protected accessor isOpened_: boolean = false;

  showDialog() {
    this.selectedTab_ = TabIndex.DEV_PROXY;
    this.installationError_ = '';
    this.isInstalling_ = false;
    this.isCurrentTabValid_ = false;
    this.isOpened_ = true;
    this.$.dialog.showModal();
  }

  closeDialog() {
    this.isOpened_ = false;
    this.$.dialog.close();
  }

  startInstallation() {
    this.$.dialog.focus();
    this.isInstalling_ = true;
    this.installationError_ = '';
  }

  onInstallationFinished(error?: string|null) {
    this.isInstalling_ = false;
    if (error) {
      this.installationError_ = error;
    } else {
      this.closeDialog();
    }
  }

  protected onCancelClick_() {
    this.isOpened_ = false;
    this.$.dialog.cancel();
  }

  protected isInstallButtonEnabled_(): boolean {
    if (this.isInstalling_) {
      return false;
    }
    return this.isCurrentTabValid_;
  }

  private get activeTab_(): IwaDevInstallTabElement|null {
    return this.shadowRoot?.querySelector<IwaDevInstallTabElement>(
               '.tab-content > *') ??
        null;
  }

  protected onInstallClick_() {
    this.installationError_ = '';
    this.activeTab_?.submit();
  }

  protected onSelectedChanged_(e: CustomEvent<{value: number}>) {
    if (this.isInstalling_) {
      return;
    }
    this.selectedTab_ = e.detail.value;
    this.installationError_ = '';
    this.isCurrentTabValid_ = false;
  }

  protected onTabValidChanged_(e: CustomEvent<{isValid: boolean}>) {
    this.isCurrentTabValid_ = e.detail.isValid;
    this.installationError_ = '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'iwa-dev-install-dialog': IwaDevInstallDialogElement;
  }
}

customElements.define(
    IwaDevInstallDialogElement.is, IwaDevInstallDialogElement);
