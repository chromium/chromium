// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-list' is a component for showing a
 * list of search engines.
 */
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';
import './search_engine_entry.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine} from './search_engines_browser_proxy.js';

Polymer({
  is: 'settings-search-engines-list',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Array<!SearchEngine>} */
    engines: Array,

    /**
     * The scroll target that this list should use.
     * @type {?HTMLElement}
     */
    scrollTarget: Object,

    /** Used to fix scrolling glitch when list is not top most element. */
    scrollOffset: Number,

    /** @private {Object}*/
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

    fixedHeight: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },
});
