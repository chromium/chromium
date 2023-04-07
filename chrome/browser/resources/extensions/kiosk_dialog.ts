// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KioskApp, KioskAppSettings, KioskBrowserProxy, KioskBrowserProxyImpl} from './kiosk_browser_proxy.js';
import {getTemplate} from './kiosk_dialog.html.js';

export interface ExtensionsKioskDialogElement {
  $: {
    addButton: CrButtonElement,
    addInput: CrInputElement,
    bailout: CrCheckboxElement,
    confirmDialog: CrDialogElement,
    dialog: CrDialogElement,
  };
}

const ExtensionsKioskDialogElementBase = WebUiListenerMixin(PolymerElement);

export class ExtensionsKioskDialogElement extends
    ExtensionsKioskDialogElementBase {
  static get is() {
    return 'extensions-kiosk-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      addAppInput_: {
        type: String,
        value: null,
      },

      apps_: Array,
      bailoutDisabled_: Boolean,
      canEditAutoLaunch_: Boolean,
      canEditBailout_: Boolean,
      errorAppId_: String,
    };
  }

  private kioskBrowserProxy_: KioskBrowserProxy =
      KioskBrowserProxyImpl.getInstance();

  private addAppInput_: string|null;
  private apps_: KioskApp[];
  private bailoutDisabled_: boolean;
  private canEditAutoLaunch_: boolean;
  private canEditBailout_: boolean;
  private errorAppId_: string|null;

  override connectedCallback() {
    super.connectedCallback();

    this.kioskBrowserProxy_.initializeKioskAppSettings()
        .then(params => {
          this.canEditAutoLaunch_ = params.autoLaunchEnabled;
          return this.kioskBrowserProxy_.getKioskAppSettings();
        })
        .then(this.setSettings_.bind(this));

    this.addWebUiListener(
        'kiosk-app-settings-changed', this.setSettings_.bind(this));
    this.addWebUiListener('kiosk-app-updated', this.updateApp_.bind(this));
    this.addWebUiListener('kiosk-app-error', this.showError_.bind(this));

    this.$.dialog.showModal();
  }

  private setSettings_(settings: KioskAppSettings) {
    this.apps_ = settings.apps;
    this.bailoutDisabled_ = settings.disableBailout;
    this.canEditBailout_ = settings.hasAutoLaunchApp;
  }

  private updateApp_(app: KioskApp) {
    const index = this.apps_.findIndex(a => a.id === app.id);
    assert(index < this.apps_.length);
    this.set('apps_.' + index, app);
  }

  private showError_(appId: string) {
    this.errorAppId_ = appId;
  }

  private getErrorMessage_(errorMessage: string): string {
    return this.errorAppId_ + ' ' + errorMessage;
  }

  private onAddAppClick_() {
    assert(this.addAppInput_);
    this.kioskBrowserProxy_.addKioskApp(this.addAppInput_);
    this.addAppInput_ = null;
  }

  private clearInputInvalid_() {
    this.errorAppId_ = null;
  }

  private onAutoLaunchButtonClick_(event: DomRepeatEvent<KioskApp>) {
    const app = event.model.item;
    if (app.autoLaunch) {  // If the app is originally set to
                           // auto-launch.
      this.kioskBrowserProxy_.disableKioskAutoLaunch(app.id);
    } else {
      this.kioskBrowserProxy_.enableKioskAutoLaunch(app.id);
    }
  }

  private onBailoutChanged_(event: Event) {
    event.preventDefault();
    if (this.$.bailout.checked) {
      this.$.confirmDialog.showModal();
    } else {
      this.kioskBrowserProxy_.setDisableBailoutShortcut(false);
      this.$.confirmDialog.close();
    }
  }

  private onBailoutDialogCancelClick_() {
    this.$.bailout.checked = false;
    this.$.confirmDialog.cancel();
  }

  private onBailoutDialogConfirmClick_() {
    this.kioskBrowserProxy_.setDisableBailoutShortcut(true);
    this.$.confirmDialog.close();
  }

  private onDoneClick_() {
    this.$.dialog.close();
  }

  private onDeleteAppClick_(event: DomRepeatEvent<KioskApp>) {
    this.kioskBrowserProxy_.removeKioskApp(event.model.item.id);
  }

  private getAutoLaunchButtonLabel_(
      autoLaunched: boolean, disableStr: string, enableStr: string): string {
    return autoLaunched ? disableStr : enableStr;
  }

  private stopPropagation_(e: Event) {
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-kiosk-dialog': ExtensionsKioskDialogElement;
  }
}


customElements.define(
    ExtensionsKioskDialogElement.is, ExtensionsKioskDialogElement);
