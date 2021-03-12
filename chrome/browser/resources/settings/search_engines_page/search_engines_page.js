// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-page' is the settings page
 * containing search engines settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/cr.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './search_engine_dialog.js';
import './search_engines_list.js';
import './omnibox_extension_entry.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.js';
import {routes} from '../route.js';

import {SearchEngine, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

Polymer({
  is: 'settings-search-engines-page',

  _template: html`{__html_template__}`,

  behaviors: [GlobalScrollTargetBehavior, WebUIListenerBehavior],

  properties: {
    /** @type {!Array<!SearchEngine>} */
    defaultEngines: Array,

    /** @type {!Array<!SearchEngine>} */
    otherEngines: Array,

    /** @type {!Array<!SearchEngine>} */
    extensions: Array,

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: routes.SEARCH_ENGINES,
    },

    /** @private {boolean} */
    showExtensionsList_: {
      type: Boolean,
      computed: 'computeShowExtensionsList_(extensions)',
    },

    /** Filters out all search engines that do not match. */
    filter: {
      type: String,
      value: '',
    },

    /** @private {!Array<!SearchEngine>} */
    matchingDefaultEngines_: {
      type: Array,
      computed: 'computeMatchingEngines_(defaultEngines, filter)',
    },

    /** @private {!Array<!SearchEngine>} */
    matchingOtherEngines_: {
      type: Array,
      computed: 'computeMatchingEngines_(otherEngines, filter)',
    },

    /** @private {!Array<!SearchEngine>} */
    matchingExtensions_: {
      type: Array,
      computed: 'computeMatchingEngines_(extensions, filter)',
    },

    /** @private {HTMLElement} */
    omniboxExtensionlastFocused_: Object,

    /** @private {boolean} */
    omniboxExtensionListBlurred_: Boolean,

    /** @private {?SearchEngine} */
    dialogModel_: {
      type: Object,
      value: null,
    },

    /** @private {?HTMLElement} */
    dialogAnchorElement_: {
      type: Object,
      value: null,
    },

    /** @private */
    showDialog_: {
      type: Boolean,
      value: false,
    },
  },

  // Since the iron-list for extensions is enclosed in a dom-if, observe both
  // |extensions| and |showExtensionsList_|.
  observers: ['extensionsChanged_(extensions, showExtensionsList_)'],

  listeners: {
    'edit-search-engine': 'onEditSearchEngine_',
  },

  /** @override */
  ready() {
    SearchEnginesBrowserProxyImpl.getInstance().getSearchEnginesList().then(
        this.enginesChanged_.bind(this));
    this.addWebUIListener(
        'search-engines-changed', this.enginesChanged_.bind(this));

    // Sets offset in iron-list that uses the page as a scrollTarget.
    afterNextRender(this, function() {
      this.$.otherEngines.scrollOffset = this.$.otherEngines.offsetTop;
    });
  },

  /**
   * @param {?SearchEngine} searchEngine
   * @param {!HTMLElement} anchorElement
   * @private
   */
  openDialog_(searchEngine, anchorElement) {
    this.dialogModel_ = searchEngine;
    this.dialogAnchorElement_ = anchorElement;
    this.showDialog_ = true;
  },

  /** @private */
  onCloseDialog_() {
    this.showDialog_ = false;
    const anchor = /** @type {!HTMLElement} */ (this.dialogAnchorElement_);
    focusWithoutInk(anchor);
    this.dialogModel_ = null;
    this.dialogAnchorElement_ = null;
  },

  /**
   * @param {!CustomEvent<!{
   *     engine: !SearchEngine,
   *     anchorElement: !HTMLElement
   * }>} e
   * @private
   */
  onEditSearchEngine_(e) {
    this.openDialog_(e.detail.engine, e.detail.anchorElement);
  },

  /** @private */
  extensionsChanged_() {
    if (this.showExtensionsList_ && this.$.extensions) {
      /** @type {!IronListElement} */ (this.$.extensions).notifyResize();
    }
  },

  /**
   * @param {!SearchEnginesInfo} searchEnginesInfo
   * @private
   */
  enginesChanged_(searchEnginesInfo) {
    this.defaultEngines = searchEnginesInfo.defaults;

    // Sort |otherEngines| in alphabetical order.
    this.otherEngines = searchEnginesInfo.others.sort(
        (a, b) => a.name.toLocaleLowerCase().localeCompare(
            b.name.toLocaleLowerCase()));

    this.extensions = searchEnginesInfo.extensions;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAddSearchEngineTap_(e) {
    e.preventDefault();
    this.openDialog_(
        null, assert(/** @type {HTMLElement} */ (this.$.addSearchEngine)));
  },

  /** @private */
  computeShowExtensionsList_() {
    return this.extensions.length > 0;
  },

  /**
   * Filters the given list based on the currently existing filter string.
   * @param {!Array<!SearchEngine>} list
   * @return {!Array<!SearchEngine>}
   * @private
   */
  computeMatchingEngines_(list) {
    if (this.filter === '') {
      return list;
    }

    const filter = this.filter.toLowerCase();
    return list.filter(e => {
      return [e.displayName, e.name, e.keyword, e.url].some(
          term => term.toLowerCase().includes(filter));
    });
  },

  /**
   * @param {!Array<!SearchEngine>} list The original list.
   * @param {!Array<!SearchEngine>} filteredList The filtered list.
   * @return {boolean} Whether to show the "no results" message.
   * @private
   */
  showNoResultsMessage_(list, filteredList) {
    return list.length > 0 && filteredList.length === 0;
  },
});
