// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../controls/extension_controlled_indicator.js';
import './search_engine_entry_css.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {FocusRowBehaviorInterface}
 */
const SettingsSearchEngineEntryElementBase =
    mixinBehaviors([FocusRowBehavior], PolymerElement);

/** @polymer */
class SettingsSearchEngineEntryElement extends
    SettingsSearchEngineEntryElementBase {
  static get is() {
    return 'settings-search-engine-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!SearchEngine} */
      engine: Object,

      /** @type {boolean} */
      isDefault: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsDefault_(engine)'
      },

    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!SearchEnginesBrowserProxy} */
    this.browserProxy_ = SearchEnginesBrowserProxyImpl.getInstance();
  }

  /** @private */
  closePopupMenu_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
  }

  /**
   * @return {boolean}
   * @private
   */
  computeIsDefault_() {
    return this.engine.default;
  }

  /** @private */
  onDeleteTap_() {
    this.browserProxy_.removeSearchEngine(this.engine.modelIndex);
    this.closePopupMenu_();
  }

  /** @private */
  onDotsTap_() {
    /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('cr-action-menu'))
        .showAt(assert(this.shadowRoot.querySelector('cr-icon-button')), {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onEditTap_(e) {
    e.preventDefault();
    this.closePopupMenu_();
    this.dispatchEvent(new CustomEvent('edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine: this.engine,
        anchorElement: assert(this.shadowRoot.querySelector('cr-icon-button')),
      },
    }));
  }

  /** @private */
  onMakeDefaultTap_() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(this.engine.modelIndex);
  }
}

customElements.define(
    SettingsSearchEngineEntryElement.is, SettingsSearchEngineEntryElement);
