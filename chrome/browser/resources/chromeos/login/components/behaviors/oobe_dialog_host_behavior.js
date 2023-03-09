// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {dom, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import { traceFirstScreenShown } from '../../oobe_trace.js';
import {invokePolymerMethod} from '../../display_manager.js';
// clang-format on

/**
 * @fileoverview
 * 'OobeDialogHostBehavior' is a behavior for oobe-dialog containers to
 * match oobe-dialog bahavior.
 */

/** @polymerBehavior */
export var OobeDialogHostBehavior = {
  properties: {},

  /**
   * Triggers onBeforeShow for descendants.
   * @suppress {missingProperties} invokePolymerMethod
   */
  propagateOnBeforeShow() {
    const dialogs = this.shadowRoot.querySelectorAll(
        'oobe-dialog,oobe-adaptive-dialog,oobe-content-dialog,' +
        'gaia-dialog,oobe-loading-dialog');
    for (const dialog of dialogs) {
      invokePolymerMethod(dialog, 'onBeforeShow');
    }
  },

  /**
   * Trigger onBeforeShow for all children.
   */
  onBeforeShow() {
    traceFirstScreenShown();
    this.propagateOnBeforeShow();
  },

  /**
   * Triggers updateLocalizedContent() for elements matched by |selector|.
   * @param {string} selector CSS selector (optional).
   */
  propagateUpdateLocalizedContent(selector) {
    var screens = dom(this.root).querySelectorAll(selector);
    for (var i = 0; i < screens.length; ++i) {
      /** @type {{updateLocalizedContent: function()}}}*/ (screens[i])
          .updateLocalizedContent();
    }
  },

};

/**
 * TODO(alemate): Replace with an interface. b/24294625
 * @typedef {{
 *   onBeforeShow: function()
 * }}
 */
OobeDialogHostBehavior.Proto;
