// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-switch-access-setup-guide-warning-dialog' is a
 * component warning the user that re-running the setup guide will clear their
 * existing switches. By clicking 'Continue', the user acknowledges that.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-switch-access-setup-guide-warning-dialog',

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onRerunSetupGuideTap_() {
    this.$.dialog.close();
  }
});
