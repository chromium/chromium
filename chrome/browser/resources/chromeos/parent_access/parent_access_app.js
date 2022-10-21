// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// strings.m.js is generated when we enable it via UseStringsJs() in webUI
// controller. When loading it, it will populate data such as localized strings
// into |window.loadTimeData|.
import './strings.m.js';
import './parent_access_after.js';
import './parent_access_ui.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
export const Screens = {
  ONLINE_FLOW: 'parent-access-ui',
  AFTER_FLOW: 'parent-access-after',
};

class ParentAccessApp extends PolymerElement {
  static get is() {
    return 'parent-access-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Specifies what the current screen is.
       * @private {Screens}
       */
      currentScreen_: {
        type: Screens,
        value: Screens.ONLINE_FLOW,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('show-after', () => {
      this.currentScreen_ = Screens.AFTER_FLOW;
      /** @type {CrViewManagerElement} */ (this.$.viewManager)
          .switchView(this.currentScreen_);
    });

    // TODO(b/200187536): Show offline screen if device is offline.
    this.currentScreen_ = Screens.ONLINE_FLOW;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
  }
}
customElements.define(ParentAccessApp.is, ParentAccessApp);