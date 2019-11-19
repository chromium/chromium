// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake out of the box experience flow (OOBE) for
 * closure_compiler coverage.
 */

/** @typedef {string} */
var ACCELERATOR_ENABLE_DEBBUGING = '1';

/** @typedef {string} */
var ACCELERATOR_DEVICE_REQUISITION_REMORA = '2';

/** @typedef {string} */
var ACCELERATOR_DEVICE_REQUISITION = '3';

var cr;
cr.ui = {};

cr.define = function(name, constructor_function) {};

/** @interface */
class Oobe {
  constructor() {}

  /** @return {Oobe} */
  getInstance() {}

  /**
   * @param {
   *     ACCELERATOR_ENABLE_DEBBUGING |
   *     ACCELERATOR_DEVICE_REQUISITION_REMORA |
   *     ACCELERATOR_DEVICE_REQUISITION
   * } accelerator
   */
  handleAccelerator(accelerator) {}

  /**
   * @param {Element} el Decorated screen element.
   * @param {DisplayManagerScreenAttributes} attributes
   */
  registerScreen(el, attributes) {}

  /**
   * @return {?OobeTypes.OobeConfiguration}
   */
  getOobeConfiguration() {}

  startDemoModeFlow() {}

  /** @type {DISPLAY_TYPE} */
  set displayType(value) {}
}

cr.ui.Oobe = Oobe;
