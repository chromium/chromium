// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
Polymer({
  is: 'settings-search-engine-entry',

  behaviors: [cr.ui.FocusRowBehavior],

  properties: {
    /** @type {!SearchEngine} */
    engine: Object,

    /** @type {boolean} */
    isDefault: {
      reflectToAttribute: true,
      type: Boolean,
      computed: 'computeIsDefault_(engine)'
    },

    /** @private {boolean} */
    showDots_: {
      reflectToAttribute: true,
      type: Boolean,
      computed: 'computeShowDots_(engine.canBeDefault,' +
          'engine.canBeEdited,' +
          'engine.canBeRemoved)',
    },
  },

  /** @private {settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('cr-action-menu').close();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsDefault_: function() {
    return this.engine.default;
  },

  /**
   * @param {boolean} canBeDefault
   * @param {boolean} canBeEdited
   * @param {boolean} canBeRemoved
   * @return {boolean} Whether to show the dots menu.
   * @private
   */
  computeShowDots_: function(canBeDefault, canBeEdited, canBeRemoved) {
    return canBeDefault || canBeEdited || canBeRemoved;
  },

  /** @private */
  onDeleteTap_: function() {
    this.browserProxy_.removeSearchEngine(this.engine.modelIndex);
    this.closePopupMenu_();
  },

  /** @private */
  onDotsTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(assert(this.$$('cr-icon-button')), {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditTap_: function(e) {
    e.preventDefault();
    this.closePopupMenu_();
    this.fire('edit-search-engine', {
      engine: this.engine,
      anchorElement: assert(this.$$('cr-icon-button')),
    });
  },

  /** @private */
  onMakeDefaultTap_: function() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(this.engine.modelIndex);
  },
});
