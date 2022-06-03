// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information for art albums.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'art-album-dialog',

  behaviors: [I18nBehavior],

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /**
   * Closes the dialog.
   * @private
   */
  onClose_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },
});
