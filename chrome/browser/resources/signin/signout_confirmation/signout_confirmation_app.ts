// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SignoutConfirmationBrowserProxyImpl} from './browser_proxy.js';
import type {SignoutConfirmationBrowserProxy} from './browser_proxy.js';
import type {SignoutConfirmationData} from './signout_confirmation.mojom-webui.js';
import {getCss} from './signout_confirmation_app.css.js';
import {getHtml} from './signout_confirmation_app.html.js';

function createDummySignoutConfirmationData(): SignoutConfirmationData {
  return {
    dialogTitle: '',
    dialogSubtitle: '',
    acceptButtonLabel: '',
    cancelButtonLabel: '',
  };
}

export interface SignoutConfirmationAppElement {
  $: {
    signoutConfirmationDialog: HTMLElement,
  };
}

export class SignoutConfirmationAppElement extends CrLitElement {
  static get is() {
    return 'signout-confirmation-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data_: {type: Object},
    };
  }

  protected data_: SignoutConfirmationData =
      createDummySignoutConfirmationData();

  private eventTracker_: EventTracker = new EventTracker();

  private signoutConfirmationBrowserProxy_: SignoutConfirmationBrowserProxy =
      SignoutConfirmationBrowserProxyImpl.getInstance();

  private onSignoutConfirmationDataReceivedListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.onSignoutConfirmationDataReceivedListenerId_ =
        this.signoutConfirmationBrowserProxy_.callbackRouter
            .sendSignoutConfirmationData.addListener(
                this.onSignoutConfirmationDataReceived_.bind(this));
    this.eventTracker_.add(window, 'keydown', this.onKeyDown_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.onSignoutConfirmationDataReceivedListenerId_);
    this.signoutConfirmationBrowserProxy_.callbackRouter.removeListener(
        this.onSignoutConfirmationDataReceivedListenerId_);
    this.onSignoutConfirmationDataReceivedListenerId_ = null;

    this.eventTracker_.removeAll();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    this.updateViewHeight_();
  }

  protected onAcceptButtonClick_() {
    this.signoutConfirmationBrowserProxy_.handler.accept();
  }

  protected onCancelButtonClick_() {
    this.signoutConfirmationBrowserProxy_.handler.cancel();
  }

  private onSignoutConfirmationDataReceived_(data: SignoutConfirmationData) {
    this.data_ = data;
  }

  // Request the browser to update the native view to match the current height
  // of the web view.
  private updateViewHeight_() {
    const height = this.$.signoutConfirmationDialog.clientHeight;
    this.signoutConfirmationBrowserProxy_.handler.updateViewHeight(height);
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.signoutConfirmationBrowserProxy_.handler.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signout-confirmation-app': SignoutConfirmationAppElement;
  }
}

customElements.define(
    SignoutConfirmationAppElement.is, SignoutConfirmationAppElement);
