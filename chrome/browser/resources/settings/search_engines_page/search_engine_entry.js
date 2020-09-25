// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../controls/extension_controlled_indicator.m.js';
import './search_engine_entry_css.js';
import '../settings_shared_css.m.js';
import '../site_favicon.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.m.js';

Polymer({
  is: 'settings-search-engine-entry',

  _template: html`{__html_template__}`,

  behaviors: [FocusRowBehavior],

  properties: {
    /** @type {!SearchEngine} */
    engine: Object,

    /** @type {boolean} */
    isDefault: {
      reflectToAttribute: true,
      type: Boolean,
      computed: 'computeIsDefault_(engine)'
    },
  },

  /** @private {SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @private */
  closePopupMenu_() {
    this.$$('cr-action-menu').close();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsDefault_() {
    return this.engine.default;
  },

  /** @private */
  onDeleteTap_() {
    this.browserProxy_.removeSearchEngine(this.engine.modelIndex);
    this.closePopupMenu_();
  },

  /** @private */
  onDotsTap_() {
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(assert(this.$$('cr-icon-button')), {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditTap_(e) {
    e.preventDefault();
    this.closePopupMenu_();
    this.fire('edit-search-engine', {
      engine: this.engine,
      anchorElement: assert(this.$$('cr-icon-button')),
    });
  },

  /** @private */
  onMakeDefaultTap_() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(this.engine.modelIndex);
  },
});
