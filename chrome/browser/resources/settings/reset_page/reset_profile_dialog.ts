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
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import type {ResetBrowserProxy} from './reset_browser_proxy.js';
import {ResetBrowserProxyImpl} from './reset_browser_proxy.js';
import {getTemplate} from './reset_profile_dialog.html.js';

export interface SettingsResetProfileDialogElement {
  $: {
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    reset: CrButtonElement,
    resetSpinner: PaperSpinnerLiteElement,
    sendSettings: CrCheckboxElement,
  };
}

const SettingsResetProfileDialogElementBase = I18nMixin(PolymerElement);

export class SettingsResetProfileDialogElement extends
    SettingsResetProfileDialogElementBase {
  static get is() {
    return 'settings-reset-profile-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(dpapad): Evaluate whether this needs to be synced across different
      // settings tabs.

      isTriggered_: {
        type: Boolean,
        value: false,
      },

      triggeredResetToolName_: {
        type: String,
        value: '',
      },

      resetRequestOrigin_: String,

      clearingInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isTriggered_: boolean;
  private triggeredResetToolName_: string;
  private resetRequestOrigin_: string;
  private clearingInProgress_: boolean;
  private browserProxy_: ResetBrowserProxy =
      ResetBrowserProxyImpl.getInstance();

  private getExplanationText_(): TrustedHTML {
    if (this.isTriggered_) {
      return this.i18nAdvanced(
          'triggeredResetPageExplanation',
          {substitutions: [this.triggeredResetToolName_]});
    }

    if (loadTimeData.getBoolean('showExplanationWithBulletPoints')) {
      return this.i18nAdvanced('resetPageExplanationBulletPoints', {
        tags: ['LINE_BREAKS', 'LINE_BREAK'],
      });
    }

    return this.i18nAdvanced('resetPageExplanation');
  }

  private getPageTitle_(): string {
    if (this.isTriggered_) {
      return loadTimeData.getStringF(
          'triggeredResetPageTitle', this.triggeredResetToolName_);
    }
    return loadTimeData.getStringF('resetDialogTitle');
  }

  override ready() {
    super.ready();

    this.addEventListener('cancel', () => {
      this.browserProxy_.onHideResetProfileDialog();
    });

    this.shadowRoot!.querySelector('cr-checkbox a')!.addEventListener(
        'click', this.onShowReportedSettingsClick_.bind(this));
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

  private onCancelClick_() {
    this.cancel();
  }

  cancel() {
    if (this.$.dialog.open) {
      this.$.dialog.cancel();
    }
  }

  private onResetClick_() {
    this.clearingInProgress_ = true;
    this.browserProxy_
        .performResetProfileSettings(
            this.$.sendSettings.checked, this.resetRequestOrigin_)
        .then(() => {
          this.clearingInProgress_ = false;
          if (this.$.dialog.open) {
            this.$.dialog.close();
          }
          this.dispatchEvent(
              new CustomEvent('reset-done', {bubbles: true, composed: true}));
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
