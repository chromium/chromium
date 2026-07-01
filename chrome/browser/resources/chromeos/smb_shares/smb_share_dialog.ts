// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
import '/strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './smb_share_dialog.html.js';

/**
 * @fileoverview
 * 'smb-share-dialog' is used to host a <add-smb-share-dialog> element to
 * add SMB file shares.
 */

const SmbShareDialogElementBase = I18nMixin(PolymerElement);

class SmbShareDialogElement extends SmbShareDialogElementBase {
  static get is() {
    return 'smb-share-dialog';
  }

  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    ColorChangeUpdater.forDocument().start();
  }

  private onDialogClose_() {
    chrome.send('dialogClose');
  }
}

customElements.define(SmbShareDialogElement.is, SmbShareDialogElement);
