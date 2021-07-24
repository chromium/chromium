// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../i18n_setup.js';

/**
 * Element that displays a header inside a module.
 * @polymer
 * @extends {PolymerElement}
 */
export class ModuleHeaderElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-module-header';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The src for the icon showing on the header.
       * @type {string}
       */
      iconSrc: String,

      /**
       * The chip text showing on the header.
       * @type {string}
       */
      chipText: String,

      /**
       * The description text showing in the header.
       * @type {string}
       */
      descriptionText: String,

      /**
       * True if the header should display an info button.
       * @type {boolean}
       */
      showInfoButton: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the redesigned modules are enabled. Will put the info
       * button in the action menu dropdown instead of separate button next to
       * the action menu.
       * @type {boolean}
       */
      showInfoButtonDropdown: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the header should display a dismiss button.
       * @type {boolean}
       */
      showDismissButton: {
        type: Boolean,
        value: false,
      },

      /** @type {string} */
      dismissText: String,

      /** @type {string} */
      disableText: String,

      /** @private */
      modulesRedesignedEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesRedesignedEnabled'),
        reflectToAttribute: true,
      },
    };
  }

  /** @private */
  onInfoButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(new Event('info-button-click', {bubbles: true}));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onMenuButtonClick_(e) {
    this.$.actionMenu.showAt(e.target);
  }

  /** @private */
  onDismissButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(new Event('dismiss-button-click', {bubbles: true}));
  }

  /** @private */
  onDisableButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(new Event('disable-button-click', {bubbles: true}));
  }

  /** @private */
  onCustomizeButtonClick_() {
    this.$.actionMenu.close();
    this.dispatchEvent(
        new Event('customize-module', {bubbles: true, composed: true}));
  }
}

customElements.define(ModuleHeaderElement.is, ModuleHeaderElement);
