// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake out of the box experience flow (OOBE) for
 * closure_compiler coverage.
 */


/** @typedef {string} */
var ACCELERATOR_DEVICE_REQUISITION_REMORA = '2';

/** @typedef {string} */
var ACCELERATOR_DEVICE_REQUISITION = '3';

var cr;
cr.ui = {};

cr.ui.login = {};

cr.ui.login.invokePolymerMethod = function(element, name, ...args) {};

cr.define = function(name, constructor_function) {};

/** @interface */
class Oobe {
  constructor() {}

  /** @return {Oobe} */
  getInstance() {}

  /**
   * @param {
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
   * @param {OOBE_UI_STATE} state
   */
  setOobeUIState(state) {}

  /**
   * @param {Object} params
   */
  showScreen(params) {}

  /**
   * @return {?OobeTypes.OobeConfiguration}
   */
  getOobeConfiguration() {}

  startDemoModeFlow() {}

  isOobeUI() {}

  /** @type {DISPLAY_TYPE} */
  set displayType(value) {}
}

cr.ui.Oobe = Oobe;
