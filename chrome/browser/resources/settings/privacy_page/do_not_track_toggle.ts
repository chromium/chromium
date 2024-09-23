// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';
import '../controls/settings_toggle_button.js';

import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {getTemplate} from './do_not_track_toggle.html.js';

export interface SettingsDoNotTrackToggleElement {
  $: {
    toggle: SettingsToggleButtonElement,
  };
}

export class SettingsDoNotTrackToggleElement extends PolymerElement {
  static get is() {
    return 'settings-do-not-track-toggle';
  }

  static get template() {
    return getTemplate();
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

      showDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  prefs: {enable_do_not_track: chrome.settingsPrivate.PrefObject};
  private showDialog_: boolean;

  private onDomChange_() {
    if (this.showDialog_) {
      this.shadowRoot!.querySelector('cr-dialog')!.showModal();
    }
  }

  /**
   * Handles the change event for the do-not-track toggle. Shows a
   * confirmation dialog when enabling the setting.
   */
  private onToggleChange_(event: Event) {
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
    this.shadowRoot!.querySelector('cr-dialog')!.close();
    this.showDialog_ = false;
  }

  private onDialogClosed_() {
    focusWithoutInk(this.toggle_);
  }

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   */
  private onDialogConfirm_() {
    this.toggle_.sendPrefChange();
    this.closeDialog_();
  }

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   */
  private onDialogCancel_() {
    this.toggle_.resetToPrefValue();
    this.closeDialog_();
  }

  private get toggle_(): SettingsToggleButtonElement {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#toggle')!;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-do-not-track-toggle': SettingsDoNotTrackToggleElement;
  }
}

customElements.define(
    SettingsDoNotTrackToggleElement.is, SettingsDoNotTrackToggleElement);
