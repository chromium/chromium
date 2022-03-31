// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './parent_access_after.js';
import './parent_access_ui.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
export const Screens = {
  ONLINE_FLOW: 'parent-access-ui',
  AFTER_FLOW: 'parent-access-after',
};

Polymer({
  is: 'parent-access-app',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Specifies what the current screen is.
     * @private {Screens}
     */
    currentScreen_: {
      type: Screens,
      value: Screens.ONLINE_FLOW,
    },
  },

  /** @override */
  ready() {
    this.addEventListener('show-after', () => {
      this.currentScreen_ = Screens.AFTER_FLOW;
      /** @type {CrViewManagerElement} */ (this.$.viewManager)
          .switchView(this.currentScreen_);
    });

    // TODO(b/200187536): Show offline screen if device is offline.
    this.currentScreen_ = Screens.ONLINE_FLOW;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
  },
});
