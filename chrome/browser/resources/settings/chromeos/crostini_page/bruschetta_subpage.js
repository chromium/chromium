// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'bruschetta-subpage' is the settings subpage for managing Bruschetta
 * (third-party VMs).
 */

import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_shared_css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteOriginBehavior, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteOriginBehaviorInterface}
 */
const BruschettaSubpageElementBase =
    mixinBehaviors([RouteOriginBehavior], PolymerElement);

/** @polymer */
class BruschettaSubpageElement extends BruschettaSubpageElementBase {
  static get is() {
    return 'settings-bruschetta-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();
    /** RouteOriginBehavior override */
    this.route_ = routes.BRUSCHETTA_DETAILS;
  }

  /** @override */
  ready() {
    super.ready();
    this.addFocusConfig(
        routes.BRUSCHETTA_SHARED_USB_DEVICES, '#bruschetta-shared-usb-devices');
  }

  /** @private */
  onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(routes.BRUSCHETTA_SHARED_USB_DEVICES);
  }
}

customElements.define(BruschettaSubpageElement.is, BruschettaSubpageElement);
