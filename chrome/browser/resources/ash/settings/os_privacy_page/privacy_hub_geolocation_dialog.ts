// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and asks users to enable system location
 * permission to allow precise timezone resolution.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../settings_shared.css.js';

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {GeolocationAccessLevel} from './privacy_hub_geolocation_subpage.js';

import {getTemplate} from './privacy_hub_geolocation_dialog.html.js';

const PrivacyHubGeolocationDialogBase = PrefsMixin(PolymerElement);

class PrivacyHubGeolocationDialog extends
    PrivacyHubGeolocationDialogBase {
  static get is() {
    return 'settings-privacy-hub-geolocation-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   */
  private onEnableClicked_(): void {
    // Send the new state immediately, this will also toggle the underlying
    // `setting-dropdown-menu` setting associated with this pref.
    this.setPrefValue(
        'ash.user.geolocation_access_level',
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    this.getDialog_().close();
  }

  private onCancelClicked_(): void {
    this.getDialog_().close();
  }

  private getDialog_(): CrDialogElement {
    return castExists(this.shadowRoot!.querySelector<CrDialogElement>(
        '#systemGeolocationDialog'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrivacyHubGeolocationDialog.is]: PrivacyHubGeolocationDialog;
  }
}

customElements.define(
    PrivacyHubGeolocationDialog.is, PrivacyHubGeolocationDialog);
