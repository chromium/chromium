// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-supported-links-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** @type {!App} */
    app: Object,
  },
});
