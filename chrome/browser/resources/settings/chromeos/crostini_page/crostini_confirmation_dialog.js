// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-confirmation-dialog' is a wrapper of
 * <cr-dialog> which
 *
 * - shows an accept button and a cancel button (you can customize the label via
 *   props);
 * - The close event has a boolean `e.detail.accepted` indicating whether the
 *   dialog is accepted or not.
 * - The dialog shows itself automatically when it is attached.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-confirmation-dialog',

  properties: {
    acceptButtonText: String,
    cancelButtonText: {
      type: String,
      value: loadTimeData.getString('cancel'),
    }
  },

  created() {
    this.accepted_ = true;
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAcceptTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onDialogCancel_(e) {
    this.accepted_ = false;
  },

  /** @private */
  onDialogClose_(e) {
    e.stopPropagation();
    this.fire('close', {'accepted': this.accepted_});
  },
});
