// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Base template with elements common to all Cellular Setup flow sub-pages. */
import '//resources/ash/common/cellular_setup/cellular_setup_icons.html.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './base_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const BasePageElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class BasePageElement extends BasePageElementBase {
  static get is() {
    return 'base-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Main title for the page.
       *
       * @type {string}
       */
      title: String,

      /**
       * Message displayed under the main title.
       *
       * @type {string}
       */
      message: String,

      /**
       * Name for the cellular-setup iconset iron-icon displayed beside message.
       *
       * @type {string}
       */
      messageIcon: {
        type: String,
        value: '',
      },
    };
  }

  /**
   * @returns {string}
   * @private
   */
  getTitle_() {
    return this.title;
  }

  /**
   * @returns {boolean}
   * @private
   */
  isTitleShown_() {
    return !!this.title;
  }

  /**
   * @returns {boolean}
   * @private
   */
  isMessageIconShown_() {
    return !!this.messageIcon;
  }
}

customElements.define(BasePageElement.is, BasePageElement);
