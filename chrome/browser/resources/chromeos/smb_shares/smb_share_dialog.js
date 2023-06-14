// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'smb-share-dialog' is used to host a <add-smb-share-dialog> element to
 * add SMB file shares.
 */

Polymer({
  is: 'smb-share-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @suppress {checkTypes}
   * @override
   */
  created() {
    ColorChangeUpdater.forDocument().start();
  },

  /** @private */
  onDialogClose_() {
    chrome.send('dialogClose');
  },
});
