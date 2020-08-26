// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-confirmation-page' component shows a confirmation
 * screen when sending data to a stranger. Strangers are devices of people that
 * are not currently in the contacts of this user.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './nearby_preview.js';
import './nearby_progress.js';
import './nearby_share_target_types.mojom-lite.js';
import './nearby_share.mojom-lite.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'nearby-confirmation-page',

  behaviors: [I18nBehavior],

  _template: html`{__html_template__}`,

  properties: {
    /**
     * ConfirmationManager interface for the currently selected share target.
     * Expected to start as null, then change to a valid object before this
     * component is shown.
     * @type {?nearbyShare.mojom.ConfirmationManagerInterface}
     */
    confirmationManager: {
      type: Object,
      value: null,
    },

    /**
     * Token to show to the user to confirm the selected share target. Expected
     * to start as null, then change to a valid object before this component is
     * shown.
     * @type {?string}
     */
    confirmationToken: {
      type: String,
      value: null,
    },

    /**
     * The selected share target to confirm the transfer for. Expected to start
     * as null, then change to a valid object before this component is shown.
     * @type {?nearbyShare.mojom.ShareTarget}
     */
    shareTarget: {
      type: Object,
      value: null,
    },
  },

  /** @private */
  onAcceptTap_() {
    this.confirmationManager.accept().then(result => {
      this.fire('close');
    });
  },

  /** @private */
  onCancelTap_() {
    this.confirmationManager.reject().then(result => {
      this.fire('close');
    });
  },
});
