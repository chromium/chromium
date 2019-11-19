// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './database_tab.js';
import './discards_tab.js';
import './graph_tab.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


Polymer({
  is: 'discards-main',

  _template: html`{__html_template__}`,

  properties: {
    selected: {
      type: Number,
      value: 0,
    },

    tabs: {
      type: Array,
      value: () => ['Discards', 'Database', 'Graph'],
    },
  },
});
