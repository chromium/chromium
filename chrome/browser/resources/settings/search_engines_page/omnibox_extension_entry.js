// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-omnibox-extension-entry' is a component for showing
 * an omnibox extension with its name and keyword.
 */
Polymer({
  is: 'settings-omnibox-extension-entry',

  properties: {
    /** @type {!SearchEngine} */
    engine: Object,
  },

  behaviors: [cr.ui.FocusRowBehavior],

  /** @private {?settings.ExtensionControlBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ =
        settings.ExtensionControlBrowserProxyImpl.getInstance();
  },

  /** @private */
  onManageTap_: function() {
    this.closePopupMenu_();
    this.browserProxy_.manageExtension(this.engine.extension.id);
  },

  /** @private */
  onDisableTap_: function() {
    this.closePopupMenu_();
    this.browserProxy_.disableExtension(this.engine.extension.id);
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('cr-action-menu').close();
  },

  /** @private */
  onDotsTap_: function() {
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(assert(this.$$('cr-icon-button')), {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  },
});
