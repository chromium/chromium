// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {KioskApp, KioskAppSettings, KioskBrowserProxy, KioskBrowserProxyImpl} from './kiosk_browser_proxy.js';

interface ExtensionsKioskDialogElement {
  $: {
    bailout: CrCheckboxElement,
    'confirm-dialog': CrDialogElement,
    dialog: CrDialogElement,
  };
}

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    item: KioskApp,
  };
}

const ExtensionsKioskDialogElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement) as
    {new (): PolymerElement & WebUIListenerBehavior};

class ExtensionsKioskDialogElement extends ExtensionsKioskDialogElementBase {
  static get is() {
    return 'extensions-kiosk-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
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
  private apps_: Array<KioskApp>;
  private bailoutDisabled_: boolean;
  private canEditAutoLaunch_: boolean;
  private canEditBailout_: boolean;
  private errorAppId_: string|null;

  connectedCallback() {
    super.connectedCallback();

    this.kioskBrowserProxy_.initializeKioskAppSettings()
        .then(params => {
          this.canEditAutoLaunch_ = params.autoLaunchEnabled;
          return this.kioskBrowserProxy_.getKioskAppSettings();
        })
        .then(this.setSettings_.bind(this));

    this.addWebUIListener(
        'kiosk-app-settings-changed', this.setSettings_.bind(this));
    this.addWebUIListener('kiosk-app-updated', this.updateApp_.bind(this));
    this.addWebUIListener('kiosk-app-error', this.showError_.bind(this));

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

  private onAddAppTap_() {
    assert(this.addAppInput_);
    this.kioskBrowserProxy_.addKioskApp(this.addAppInput_!);
    this.addAppInput_ = null;
  }

  private clearInputInvalid_() {
    this.errorAppId_ = null;
  }

  private onAutoLaunchButtonTap_(event: RepeaterEvent) {
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
      this.$['confirm-dialog'].showModal();
    } else {
      this.kioskBrowserProxy_.setDisableBailoutShortcut(false);
      this.$['confirm-dialog'].close();
    }
  }

  private onBailoutDialogCancelTap_() {
    this.$.bailout.checked = false;
    this.$['confirm-dialog'].cancel();
  }

  private onBailoutDialogConfirmTap_() {
    this.kioskBrowserProxy_.setDisableBailoutShortcut(true);
    this.$['confirm-dialog'].close();
  }

  private onDoneTap_() {
    this.$.dialog.close();
  }

  private onDeleteAppTap_(event: RepeaterEvent) {
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

customElements.define(
    ExtensionsKioskDialogElement.is, ExtensionsKioskDialogElement);
