// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a hotspot summary item row with
 * a toggle button below the network summary item.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const HotspotSummaryItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class HotspotSummaryItemElement extends HotspotSummaryItemElementBase {
  static get is() {
    return 'hotspot-summary-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @private */
  onSubpageArrowClick_() {
    // TODO: implementation
  }

  /** @private */
  onWrapperClick_() {
    // TODO: implementation
  }
}

customElements.define(HotspotSummaryItemElement.is, HotspotSummaryItemElement);
