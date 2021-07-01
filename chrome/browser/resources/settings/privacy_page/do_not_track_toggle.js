// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../settings_shared_css.js';
import '../controls/settings_toggle_button.js';

import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';


/** @polymer */
export class SettingsDoNotTrackToggleElement extends PolymerElement {
  static get is() {
    return 'settings-do-not-track-toggle';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private */
      showDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @private */
  onDomChange_() {
    if (this.showDialog_) {
      this.shadowRoot.querySelector('#confirmDialog').showModal();
    }
  }

  /**
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   * @param {!Event} event
   * @private
   */
  onToggleChange_(event) {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.DO_NOT_TRACK);
    const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
    if (!target.checked) {
      // Always allow disabling the pref.
      target.sendPrefChange();
      return;
    }

    this.showDialog_ = true;
  }

  /** @private */
  closeDialog_() {
    this.shadowRoot.querySelector('#confirmDialog').close();
    this.showDialog_ = false;
  }

  /** @private */
  onDialogClosed_() {
    focusWithoutInk(this.$.toggle);
  }

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onDialogConfirm_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.toggle)
        .sendPrefChange();
    this.closeDialog_();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onDialogCancel_() {
    /** @type {!SettingsToggleButtonElement} */ (this.$.toggle)
        .resetToPrefValue();
    this.closeDialog_();
  }
}

customElements.define(
    SettingsDoNotTrackToggleElement.is, SettingsDoNotTrackToggleElement);
