// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StringUtil} from '/common/string_util.js';

/**
 * Class to record metrics for Switch Access.
 */
export namespace SwitchAccessMetrics {
  export function recordMenuAction(menuAction: string): void {
    const metricName = 'Accessibility.CrosSwitchAccess.MenuAction.' +
        StringUtil.toUpperCamelCase(menuAction);
    chrome.metricsPrivate.recordUserAction(metricName);
  }
}
