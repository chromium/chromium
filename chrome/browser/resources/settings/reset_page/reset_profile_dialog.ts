// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-reset-profile-dialog' is the dialog shown for clearing profile
 * settings. A triggered variant of this dialog can be shown under certain
 * circumstances. See triggered_profile_resetter.h for when the triggered
 * variant will be used.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/js/action_link.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import type {ResetBrowserProxy} from './reset_browser_proxy.js';
import {ResetBrowserProxyImpl} from './reset_browser_proxy.js';
import {getCss} from './reset_profile_dialog.css.js';
import {getHtml} from './reset_profile_dialog.html.js';

export interface SettingsResetProfileDialogElement {
  $: {
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    reset: CrButtonElement,
  };
}

const SettingsResetProfileDialogElementBase = I18nMixinLit(CrLitElement);

export class SettingsResetProfileDialogElement extends
    SettingsResetProfileDialogElementBase {
  static get is() {
    return 'settings-reset-profile-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // TODO(dpapad): Evaluate whether this needs to be synced across different
      // settings tabs.

      isTriggered_: {type: Boolean},
      triggeredResetToolName_: {type: String},
      resetRequestOrigin_: {type: String},
      clearingInProgress_: {type: Boolean},
    };
  }

  private accessor isTriggered_: boolean = false;
  private accessor triggeredResetToolName_: string = '';
  private accessor resetRequestOrigin_: string = '';
  protected accessor clearingInProgress_: boolean = false;

  protected readonly settingsRefresh2026Enabled_: boolean =
      loadTimeData.getString('settingsRefresh2026') !== '';
  private browserProxy_: ResetBrowserProxy =
      ResetBrowserProxyImpl.getInstance();

  override firstUpdated() {
    this.addEventListener('cancel', () => {
      this.browserProxy_.onHideResetProfileDialog();
    });

    this.shadowRoot.querySelector('cr-checkbox a')!.addEventListener(
        'click', this.onShowReportedSettingsClick_.bind(this));
  }

  protected getExplanationText_(): TrustedHTML {
    if (this.isTriggered_) {
      return this.i18nAdvanced(
          'triggeredResetPageExplanation',
          {substitutions: [this.triggeredResetToolName_]});
    }

    return this.i18nAdvanced('resetPageExplanationBulletPoints', {
      tags: ['LINE_BREAKS', 'LINE_BREAK'],
    });
  }

  protected getCancelButtonClass_(): string {
    const classes = ['cancel-button'];
    if (this.settingsRefresh2026Enabled_) {
      classes.push('tonal-button');
    }
    return classes.join(' ');
  }

  protected getPageTitle_(): string {
    if (this.isTriggered_) {
      return loadTimeData.getStringF(
          'triggeredResetPageTitle', this.triggeredResetToolName_);
    }
    return loadTimeData.getStringF('resetDialogTitle');
  }

  private showDialog_() {
    if (!this.$.dialog.open) {
      this.$.dialog.showModal();
    }
    this.browserProxy_.onShowResetProfileDialog();
  }

  show() {
    this.isTriggered_ = Router.getInstance().getCurrentRoute() ===
        routes.TRIGGERED_RESET_DIALOG;
    if (this.isTriggered_) {
      this.browserProxy_.getTriggeredResetToolName().then(name => {
        this.resetRequestOrigin_ = 'triggeredreset';
        this.triggeredResetToolName_ = name;
        this.showDialog_();
      });
    } else {
      this.resetRequestOrigin_ =
          Router.getInstance().getQueryParameters().get('origin') || '';
      this.showDialog_();
    }
  }

  protected onCancelClick_() {
    this.cancel();
  }

  cancel() {
    if (this.$.dialog.open) {
      this.$.dialog.cancel();
    }
  }

  protected onResetClick_() {
    this.clearingInProgress_ = true;
    const sendSettings =
        this.shadowRoot.querySelector<CrCheckboxElement>('#sendSettings');
    assert(sendSettings);
    this.browserProxy_
        .performResetProfileSettings(
            sendSettings.checked, this.resetRequestOrigin_)
        .then(() => {
          this.clearingInProgress_ = false;
          if (this.$.dialog.open) {
            this.$.dialog.close();
          }
          this.fire('reset-done');
        });
  }

  /**
   * Displays the settings that will be reported in a new tab.
   */
  private onShowReportedSettingsClick_(e: Event) {
    this.browserProxy_.showReportedSettings();
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-reset-profile-dialog': SettingsResetProfileDialogElement;
  }
}

customElements.define(
    SettingsResetProfileDialogElement.is, SettingsResetProfileDialogElement);
