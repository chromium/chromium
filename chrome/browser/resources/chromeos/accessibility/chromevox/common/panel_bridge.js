// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox panel.
 */

goog.provide('PanelBridge');

PanelBridge = {
  async onCurrentRangeChanged() {
    return BridgeHelper.sendMessage(
        BridgeTarget.PANEL, BridgeAction.ON_CURRENT_RANGE_CHANGED);
  },
};
