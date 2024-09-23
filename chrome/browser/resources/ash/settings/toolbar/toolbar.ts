// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '../os_settings_search_box/os_settings_search_box.js';
import '../settings_shared.css.js';

import {CrToolbarSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './toolbar.html.js';

export class SettingsToolbarElement extends PolymerElement {
  static get is() {
    return 'settings-toolbar';
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
       */
      isSearchBoxCutoff_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      showingSearch_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  spinnerActive: boolean;
  showMenu: boolean;
  showSearch: boolean;
  narrow: boolean;
  private isSearchBoxCutoff_: boolean;
  private showingSearch_: boolean;

  getSearchField(): CrToolbarSearchFieldElement {
    const searchBox =
        castExists(this.shadowRoot!.querySelector('os-settings-search-box'));
    return castExists(
        searchBox.shadowRoot!.querySelector('cr-toolbar-search-field'));
  }

  private onMenuClick_(): void {
    const event =
      new CustomEvent('settings-toolbar-menu-tap',
                      {bubbles: true, composed: true});
    this.dispatchEvent(event);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-toolbar': SettingsToolbarElement;
  }
}

customElements.define(SettingsToolbarElement.is, SettingsToolbarElement);
