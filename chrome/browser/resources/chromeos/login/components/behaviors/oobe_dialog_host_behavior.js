// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dom} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {invokePolymerMethod} from '../../display_manager.js';
import {traceFirstScreenShown} from '../../oobe_trace.js';

/**
 * @fileoverview
 * 'OobeDialogHostBehavior' is a behavior for oobe-dialog containers to
 * match oobe-dialog bahavior.
 */

/** @polymerBehavior */
export const OobeDialogHostBehavior = {
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
    const screens = dom(this.root).querySelectorAll(selector);
    for (let i = 0; i < screens.length; ++i) {
      /** @type {{updateLocalizedContent: function()}}}*/ (screens[i])
          .updateLocalizedContent();
    }
  },

};

/** @interface */
export class OobeDialogHostBehaviorInterface {
  /** @param {...Object} data  */
  onBeforeShow(...data) {}
}
