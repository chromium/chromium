// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to record metrics for Switch Access.
 */
export const SwitchAccessMetrics = {
  /**
   * @param {string} menuAction
   */
  recordMenuAction: menuAction => {
    const metricName = 'Accessibility.CrosSwitchAccess.MenuAction.' +
        SwitchAccessMetrics.toUpperCamelCase(menuAction);
    chrome.metricsPrivate.recordUserAction(metricName);
  },

  /**
   * @param {string} str
   * @return {string}
   */
  toUpperCamelCase: str => {
    const wordRegex = /(?:^\w|[A-Z]|(?:\b|_)\w)/g;
    const underscoreAndWhitespaceRegex = /(\s|_)+/g;
    return str.replace(wordRegex, word => word.toUpperCase())
        .replace(underscoreAndWhitespaceRegex, '');
  },
};
