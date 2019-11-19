// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to record metrics for Switch Access.
 */
const SwitchAccessMetrics = {
  /**
   * @param {string} action
   */
  recordMenuAction: (action) => {
    let metricName = 'Accessibility.CrosSwitchAccess.MenuAction.' +
        SwitchAccessMetrics.toUpperCamelCase(action);
    chrome.metricsPrivate.recordUserAction(metricName);
  },

  /**
   * @param {string} str
   * @return {string}
   */
  toUpperCamelCase: (str) => {
    const wordRegex = /(?:^\w|[A-Z]|(?:\b|_)\w)/g;
    const underscoreAndWhitespaceRegex = /(\s|_)+/g;
    return str.replace(wordRegex, (word) => word.toUpperCase())
        .replace(underscoreAndWhitespaceRegex, '');
  }
};
