// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './topics_consent_notice.js';
import './protected_audience_measurement_notice.js';
import './three_ads_apis_notice.js';
import './measurement_notice.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {BaseDialogPageHandlerInterface} from './base_dialog.mojom-webui.js';
import {getCss} from './base_dialog_app.css.js';
import {getHtml} from './base_dialog_app.html.js';
import {BaseDialogBrowserProxy} from './base_dialog_browser_proxy.js';
import {PrivacySandboxNotice} from './notice.mojom-webui.js';

export interface BaseDialogApp {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export class BaseDialogApp extends CrLitElement {
  static get is() {
    return 'base-dialog-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private proxy_: BaseDialogBrowserProxy = BaseDialogBrowserProxy.getInstance();
  private handler_: BaseDialogPageHandlerInterface;
  private navigateToNextStepListenerId_: number|null = null;

  override firstUpdated() {
    this.handler_ = this.proxy_.handler;
    this.navigateToStep_(
            loadTimeData.getInteger('noticeIdToShow') as PrivacySandboxNotice)
        .then(() => this.resizeAndShowNativeDialog());
  }

  override connectedCallback() {
    super.connectedCallback();
    // Once the `callbackRouter` is notified that `navigateToNextStep` is
    // triggered, we should switch views within this dialog.
    this.navigateToNextStepListenerId_ =
        this.proxy_.callbackRouter.navigateToNextStep.addListener(
            this.navigateToStep_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.navigateToNextStepListenerId_);
    this.proxy_.callbackRouter.removeListener(
        this.navigateToNextStepListenerId_);
  }

  private navigateToStep_(step: PrivacySandboxNotice): Promise<void> {
    return this.$.viewManager.switchView(
        this.getNoticeId(step), 'fade-in', 'fade-out');
  }

  /**
   * Converts a PrivacySandboxNotice enum value (number) into its corresponding
   * string key/name (e.g., 0 becomes "kTopicsConsentNotice").
   * Used to generate unique string IDs for the different dialog steps.
   */
  protected getNoticeId(step: PrivacySandboxNotice): string {
    return PrivacySandboxNotice[step];
  }

  private resizeAndShowNativeDialog(): Promise<void> {
    return new Promise(async resolve => {
      // Prefer using |document.body.offsetHeight| instead of
      // |document.body.scrollHeight| as it returns the correct height
      // of the page even when the page zoom in Chrome is different
      // than 100%.
      await this.handler_.resizeDialog(document.body.offsetHeight);
      // After the layout is adjusted to fit into the dialog, show
      // the native dialog.
      this.handler_.showDialog();
      resolve();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'base-dialog-app': BaseDialogApp;
  }
}

customElements.define(BaseDialogApp.is, BaseDialogApp);
