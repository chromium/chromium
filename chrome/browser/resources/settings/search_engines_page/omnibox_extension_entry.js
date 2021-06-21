// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-omnibox-extension-entry' is a component for showing
 * an omnibox extension with its name and keyword.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './search_engine_entry_css.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionControlBrowserProxy, ExtensionControlBrowserProxyImpl} from '../extension_control_browser_proxy.js';

import {SearchEngine} from './search_engines_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {FocusRowBehaviorInterface}
 */
const SettingsOmniboxExtensionEntryElementBase =
    mixinBehaviors([FocusRowBehavior], PolymerElement);

/** @polymer */
class SettingsOmniboxExtensionEntryElement extends
    SettingsOmniboxExtensionEntryElementBase {
  static get is() {
    return 'settings-omnibox-extension-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!SearchEngine} */
      engine: Object,
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!ExtensionControlBrowserProxy} */
    this.browserProxy_ = ExtensionControlBrowserProxyImpl.getInstance();
  }

  /** @private */
  onManageTap_() {
    this.closePopupMenu_();
    this.browserProxy_.manageExtension(this.engine.extension.id);
  }

  /** @private */
  onDisableTap_() {
    this.closePopupMenu_();
    this.browserProxy_.disableExtension(this.engine.extension.id);
  }

  /** @private */
  closePopupMenu_() {
    this.shadowRoot.querySelector('cr-action-menu').close();
  }

  /** @private */
  onDotsTap_() {
    /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('cr-action-menu'))
        .showAt(assert(this.shadowRoot.querySelector('cr-icon-button')), {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  }
}

customElements.define(
    SettingsOmniboxExtensionEntryElement.is,
    SettingsOmniboxExtensionEntryElement);
