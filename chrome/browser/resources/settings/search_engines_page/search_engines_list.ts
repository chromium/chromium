// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-list' is a component for showing a
 * list of search engines.
 */
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import './search_engine_entry.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SearchEngine} from './search_engines_browser_proxy.js';
import {getTemplate} from './search_engines_list.html.js';

export class SettingsSearchEnginesListElement extends PolymerElement {
  static get is() {
    return 'settings-search-engines-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      engines: Array,

      showShortcut: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showQueryUrl: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      collapseList: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * The number of engines visible when the list is collapsed.
       */
      visibleEnginesSize: {
        type: Number,
        value: 5,
      },

      /**
       * An array of the first 'visibleEnginesSize' engines in the `engines`
       * array.  These engines are visible even when 'collapsedEngines' is
       * collapsed.
       */
      visibleEngines:
          {type: Array, computed: 'computeVisibleEngines_(engines)'},

      /**
       * An array of all remaining engines not in the `visibleEngines` array.
       * These engines' visibility can be toggled by expanding or collapsing the
       * engines list.
       */
      collapsedEngines:
          {type: Array, computed: 'computeCollapsedEngines_(engines)'},

      /** Used to fix scrolling glitch when list is not top most element. */
      scrollOffset: Number,

      lastFocused_: Object,

      listBlurred_: Boolean,

      expandListText: {
        type: String,
        reflectToAttribute: true,
      },

      fixedHeight: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  engines: SearchEngine[];
  visibleEngines: SearchEngine[];
  collapsedEngines: SearchEngine[];
  visibleEnginesSize: number;
  fixedHeight: boolean;
  showShortcut: boolean;
  showQueryUrl: boolean;
  collapseList: boolean;
  expandListText: string;
  private lastFocused_: HTMLElement;
  private listBlurred_: boolean;

  private computeVisibleEngines_(engines: SearchEngine[]) {
    if (!engines || !engines.length) {
      return;
    }

    return engines.slice(0, this.visibleEnginesSize);
  }

  private computeCollapsedEngines_(engines: SearchEngine[]) {
    if (!engines || !engines.length) {
      return;
    }

    return engines.slice(this.visibleEnginesSize);
  }
}

customElements.define(
    SettingsSearchEnginesListElement.is, SettingsSearchEnginesListElement);
