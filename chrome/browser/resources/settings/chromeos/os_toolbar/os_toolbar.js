// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '../os_settings_search_box/os_settings_search_box.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './os_toolbar.html.js';

/** @polymer */
class OsToolbarElement extends PolymerElement {
  static get is() {
    return 'os-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Value is proxied through to cr-toolbar-search-field. When true,
      // the search field will show a processing spinner.
      spinnerActive: Boolean,

      // Controls whether the menu button is shown at the start of the menu.
      showMenu: {type: Boolean, value: false},

      // Controls whether the search field is shown.
      showSearch: {type: Boolean, value: true},

      // True when the toolbar is displaying in narrow mode.
      narrow: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * True when the toolbar is displaying in an extremely narrow mode that
       * the viewport may cutoff an OsSettingsSearchBox with a specific px
       * width.
       * @private
       */
      isSearchBoxCutoff_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** @private */
      showingSearch_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  /** @return {?CrToolbarSearchFieldElement} */
  getSearchField() {
    return /** @type {?CrToolbarSearchFieldElement} */ (
        this.shadowRoot.querySelector('os-settings-search-box')
            .shadowRoot.querySelector('cr-toolbar-search-field'));
  }

  /** @private */
  onMenuTap_() {
    const event =
        new CustomEvent('os-toolbar-menu-tap', {bubbles: true, composed: true});
    this.dispatchEvent(event);
  }
}

customElements.define(OsToolbarElement.is, OsToolbarElement);
