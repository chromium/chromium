// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';
import './extensions_section.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SignoutConfirmationBrowserProxyImpl} from './browser_proxy.js';
import type {SignoutConfirmationBrowserProxy} from './browser_proxy.js';
import type {ExtensionsSectionElement} from './extensions_section.js';
import type {SignoutConfirmationData} from './signout_confirmation.mojom-webui.js';
import {getCss} from './signout_confirmation_app.css.js';
import {getHtml} from './signout_confirmation_app.html.js';

const SAMPLE_DATA: SignoutConfirmationData = {
  dialogTitle: '',
  dialogSubtitle: '',
  acceptButtonLabel: '',
  cancelButtonLabel: '',
  accountExtensions: [],
  hasUnsyncedData: false,
};

export interface SignoutConfirmationAppElement {
  $: {
    signoutConfirmationDialog: HTMLElement,
    acceptButton: CrButtonElement,
    cancelButton: CrButtonElement,
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

  protected accessor data_: SignoutConfirmationData = SAMPLE_DATA;

  private eventTracker_: EventTracker = new EventTracker();

  private signoutConfirmationBrowserProxy_: SignoutConfirmationBrowserProxy =
      SignoutConfirmationBrowserProxyImpl.getInstance();

  private onSignoutConfirmationDataReceivedListenerId_: number|null = null;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.onSignoutConfirmationDataReceivedListenerId_ =
        this.signoutConfirmationBrowserProxy_.callbackRouter
            .sendSignoutConfirmationData.addListener(
                this.onSignoutConfirmationDataReceived_.bind(this));
    this.eventTracker_.add(window, 'keydown', this.onKeyDown_.bind(this));
    this.eventTracker_.add(
        window, 'update-view-height', this.onUpdateViewHeight_.bind(this));
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

    // Cast necessary since the properties are protected.
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    // Avoid requesting a view update if we are still using the sample data that
    // was set at construction. Since the first view update will trigger showing
    // the view, we should make sure to have valid data to show.
    if (changedPrivateProperties.has('data_') && this.data_ !== SAMPLE_DATA) {
      this.onUpdateViewHeight_();
    }
  }

  uninstallExtensionsOnSignoutForTesting(): boolean {
    return this.uninstallExtensionsOnSignout_();
  }

  protected showExtensionsSection_(): boolean {
    return !!this.data_.accountExtensions.length;
  }

  // Returns if additional text should be shown in the dialog if the user has
  // account extensions installed.
  protected showExtensionsAdditionalText_(): boolean {
    return this.showExtensionsSection_() && this.data_.hasUnsyncedData;
  }

  protected onAcceptButtonClick_() {
    this.signoutConfirmationBrowserProxy_.handler.accept(
        this.uninstallExtensionsOnSignout_());
  }

  protected onCancelButtonClick_() {
    this.signoutConfirmationBrowserProxy_.handler.cancel(
        this.uninstallExtensionsOnSignout_());
  }

  // Request the browser to update the native view to match the current height
  // of the web view.
  private onUpdateViewHeight_() {
    const height = this.$.signoutConfirmationDialog.clientHeight;
    this.signoutConfirmationBrowserProxy_.handler.updateViewHeight(height);
  }

  private onSignoutConfirmationDataReceived_(data: SignoutConfirmationData) {
    this.data_ = data;
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      this.signoutConfirmationBrowserProxy_.handler.close();
    }
  }

  // Returns Whether account extensions should be uninstalled when the user
  // signs out from the dialog.
  private uninstallExtensionsOnSignout_(): boolean {
    const extensionsSection =
        this.shadowRoot.querySelector<ExtensionsSectionElement>(
            'extensions-section');
    return !!extensionsSection && extensionsSection.checked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signout-confirmation-app': SignoutConfirmationAppElement;
  }
}

customElements.define(
    SignoutConfirmationAppElement.is, SignoutConfirmationAppElement);
