// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_shared_style.css.js';
import './toggle_row.js';

import {assert} from '//resources/js/assert.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './file_handling_item.css.js';
import {getHtml} from './file_handling_item.html.js';
import type {ToggleRowElement} from './toggle_row.js';
import {createDummyApp} from './web_app_settings_utils.js';

const FileHandlingItemBase = I18nMixinLit(CrLitElement);

export class FileHandlingItemElement extends FileHandlingItemBase {
  static get is() {
    return 'app-management-file-handling-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
      showOverflowDialog: {type: Boolean},
      hidden: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  app: App = createDummyApp();
  showOverflowDialog: boolean = false;
  override hidden: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('app')) {
      this.hidden = this.isHidden_();
    }
  }

  override firstUpdated() {
    this.addEventListener('change', this.onChanged_);
  }

  private isHidden_(): boolean {
    if (this.app.fileHandlingState) {
      return !this.app.fileHandlingState.userVisibleTypes;
    }
    return false;
  }

  protected isManaged_(): boolean {
    if (this.app.fileHandlingState) {
      return this.app.fileHandlingState.isManaged;
    }
    return false;
  }

  protected userVisibleTypes_(): string {
    if (this.app.fileHandlingState) {
      return this.app.fileHandlingState.userVisibleTypes;
    }
    return '';
  }

  protected userVisibleTypesLabel_(): string {
    if (this.app.fileHandlingState) {
      return this.app.fileHandlingState.userVisibleTypesLabel;
    }
    return '';
  }

  protected getLearnMoreLinkUrl_(): string {
    if (this.app.fileHandlingState && this.app.fileHandlingState.learnMoreUrl) {
      return this.app.fileHandlingState.learnMoreUrl.url;
    }
    return '';
  }

  protected onLearnMoreLinkClicked_(e: CustomEvent): void {
    if (!this.getLearnMoreLinkUrl_()) {
      // Currently, this branch should only be used on Windows.
      e.detail.event.preventDefault();
      e.stopPropagation();
      BrowserProxy.getInstance().handler.showDefaultAppAssociationsUi();
    }
  }

  protected launchDialog_(e: CustomEvent): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.showOverflowDialog = true;

    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.FILE_HANDLING_OVERFLOW_SHOWN);
  }

  protected onCloseButtonClicked_() {
    this.shadowRoot!.querySelector<CrDialogElement>('#dialog')!.close();
  }

  protected onDialogClose_(): void {
    this.showOverflowDialog = false;
    const toFocus = this.shadowRoot!.querySelector<HTMLElement>('#type-list');
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  protected getValue_(): boolean {
    if (this.app.fileHandlingState) {
      return this.app.fileHandlingState.enabled;
    }
    return false;
  }

  private onChanged_() {
    assert(this.app);
    const enabled =
        this.shadowRoot!.querySelector<ToggleRowElement>(
                            '#toggle-row')!.isChecked();

    BrowserProxy.getInstance().handler.setFileHandlingEnabled(
        this.app.id,
        enabled,
    );
    const fileHandlingChangeAction = enabled ?
        AppManagementUserAction.FILE_HANDLING_TURNED_ON :
        AppManagementUserAction.FILE_HANDLING_TURNED_OFF;
    recordAppManagementUserAction(this.app.type, fileHandlingChangeAction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-file-handling-item': FileHandlingItemElement;
  }
}

customElements.define(FileHandlingItemElement.is, FileHandlingItemElement);
