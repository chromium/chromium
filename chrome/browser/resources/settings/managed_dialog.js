// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'managed-dialog' is a dialog that is displayed when a user
 * interact with some UI features which are managed by the user's organization.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-managed-dialog',

  _template: html`{__html_template__}`,

  properties: {
    /** Managed dialog title text. */
    title: String,

    /** Managed dialog body text. */
    body: String,
  },

  /** @private */
  onOkClick_() {
    this.$.dialog.close();
  },
});
