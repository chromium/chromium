// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and asks users to enable system location
 * permission to allow precise timezone resolution.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './geolocation_dialog.html.js';
import {logSystemLocationPermissionChange} from './personalization_metrics_logger.js';

/**
 * Geolocation access levels for the ChromeOS system.
 * This must be kept in sync with `GeolocationAccessLevel` in
 * ash/constants/geolocation_access_level.h
 */
export enum GeolocationAccessLevel {
  DISALLOWED = 0,
  ALLOWED = 1,
  ONLY_ALLOWED_FOR_SYSTEM = 2,

  MAX_VALUE = ONLY_ALLOWED_FOR_SYSTEM,
}

class GeolocationDialog extends PolymerElement {
  static get is() {
    return 'geolocation-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      bodyTextParagraph1: {
        type: String,
      },
      bodyTextParagraph2: {
        type: String,
      },
      cancelButtonText: {
        type: String,
      },
      confirmButtonText: {
        type: String,
      },
    };
  }

  bodyText: string;
  cancelButtonText: string;
  confirmButtonText: string;

  /**
   * Callback on user accepting the geolocation dialog, with the intent to
   * enable geolocation for system services.
   * Closes the warning dialog and emits 'geolocation-enabled' event for direct
   * parent element to process.
   */
  private onEnableClicked_(): void {
    this.dispatchEvent(
        new CustomEvent('geolocation-enabled', {bubbles: false}));

    logSystemLocationPermissionChange(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    this.getDialog_().close();
  }

  // Closes the dialog.
  private onCancelClicked_(): void {
    this.getDialog_().close();
  }

  private getDialog_(): CrDialogElement {
    return strictQuery(
        '#systemGeolocationDialog', this.shadowRoot, CrDialogElement);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GeolocationDialog.is]: GeolocationDialog;
  }
}

customElements.define(GeolocationDialog.is, GeolocationDialog);
