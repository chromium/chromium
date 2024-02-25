// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from ui/webui/resources/cr_elements/cr_toolbar/cr_toolbar.ts

import '../cr_icon_button/cr_icon_button.js';
import '../cr_icons.css.js';
import '../cr_hidden_style.css.js';
import '../cr_shared_vars.css.js';
import '../icons.html.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './cr_toolbar_search_field.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_toolbar.html.js';
import {CrToolbarSearchFieldElement} from './cr_toolbar_search_field.js';

export interface CrToolbarElement {
  $: {
    search: CrToolbarSearchFieldElement,
  };
}

export class CrToolbarElement extends PolymerElement {
  static get is() {
    return 'cr-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Name to display in the toolbar, in titlecase.
      pageName: String,

      // Prompt text to display in the search field.
      searchPrompt: String,

      // Tooltip to display on the clear search button.
      clearLabel: String,

      // Tooltip to display on the menu button.
      menuLabel: String,

      // Value is proxied through to cr-toolbar-search-field. When true,
      // the search field will show a processing spinner.
      spinnerActive: Boolean,

      // Controls whether the menu button is shown at the start of the menu.
      showMenu: {type: Boolean, value: false},

      // Controls whether the search field is shown.
      showSearch: {type: Boolean, value: true},

      // Controls whether the search field is autofocused.
      autofocus: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // True when the toolbar is displaying in narrow mode.
      narrow: {
        type: Boolean,
        reflectToAttribute: true,
        readonly: true,
        notify: true,
      },

      /**
       * The threshold at which the toolbar will change from normal to narrow
       * mode, in px.
       */
      narrowThreshold: {
        type: Number,
        value: 900,
      },

      alwaysShowLogo: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showingSearch_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  pageName: string;
  searchPrompt: string;
  clearLabel: string;
  menuLabel: string;
  spinnerActive: boolean;
  showMenu: boolean;
  showSearch: boolean;
  override autofocus: boolean;
  narrow: boolean;
  narrowThreshold: number;
  alwaysShowLogo: boolean;
  private showingSearch_: boolean;

  getSearchField(): CrToolbarSearchFieldElement {
    return this.$.search;
  }

  private onMenuClick_() {
    this.dispatchEvent(new CustomEvent(
        'cr-toolbar-menu-click', {bubbles: true, composed: true}));
  }

  focusMenuButton() {
    requestAnimationFrame(() => {
      // Wait for next animation frame in case dom-if has not applied yet and
      // added the menu button.
      const menuButton =
          this.shadowRoot!.querySelector<HTMLElement>('#menuButton');
      if (menuButton) {
        menuButton.focus();
      }
    });
  }

  isMenuFocused(): boolean {
    return !!this.shadowRoot!.activeElement &&
        this.shadowRoot!.activeElement.id === 'menuButton';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar': CrToolbarElement;
  }
}

customElements.define(CrToolbarElement.is, CrToolbarElement);
