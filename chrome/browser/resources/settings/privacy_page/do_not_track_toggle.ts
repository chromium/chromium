// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {getCss} from './do_not_track_toggle.css.js';
import {getHtml} from './do_not_track_toggle.html.js';

export interface SettingsDoNotTrackToggleElement {
  $: {
    toggle: SettingsToggleButtonElement,
  };
}

const SettingsDoNotTrackToggleElementBase = I18nMixinLit(CrLitElement);

export class SettingsDoNotTrackToggleElement extends
    SettingsDoNotTrackToggleElementBase {
  static get is() {
    return 'settings-do-not-track-toggle';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showDialog_: {type: Boolean},
      showUniversalOptOutSettings_: {type: Boolean},
      doNotTrackSublabel_: {type: String},
    };
  }

  protected accessor showDialog_: boolean = false;
  protected accessor showUniversalOptOutSettings_: boolean =
      loadTimeData.getBoolean('showUniversalOptOutSettings');
  protected accessor doNotTrackSublabel_: string = '';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('showUniversalOptOutSettings_')) {
      this.doNotTrackSublabel_ = this.computeDoNotTrackToggleSubLabel_();
    }
  }

  /**
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   */
  protected onSettingsBooleanControlChange_(event: Event) {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.DO_NOT_TRACK);
    const target = event.target as SettingsToggleButtonElement;
    if (!target.checked) {
      // Always allow disabling the pref.
      target.sendPrefChange();
      return;
    }

    this.showDialog_ = true;
  }

  private closeDialog_() {
    this.shadowRoot.querySelector('cr-dialog')!.close();
    this.showDialog_ = false;
  }

  protected onDialogClose_() {
    focusWithoutInk(this.$.toggle);
  }

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   */
  protected onConfirmClick_() {
    this.$.toggle.sendPrefChange();
    this.closeDialog_();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button.
   */
  protected onCancelClick_() {
    this.$.toggle.resetToPrefValue();
    this.closeDialog_();
  }

  /**
   * Handles the shared proxy confirmation dialog cancel event.
   */
  protected onDialogCancel_() {
    this.onCancelClick_();
  }

  private computeDoNotTrackToggleSubLabel_(): string {
    return this.i18n(
        this.showUniversalOptOutSettings_ ?
            'trackingProtectionDoNotTrackDisclaimerToggleSubLabel' :
            'trackingProtectionDoNotTrackToggleSubLabel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-do-not-track-toggle': SettingsDoNotTrackToggleElement;
  }
}

customElements.define(
    SettingsDoNotTrackToggleElement.is, SettingsDoNotTrackToggleElement);
