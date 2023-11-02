// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from './assert.js';

/**
 * CSS rules.
 */
const cssRules: CSSRule[] = (() => {
  const ruleList = [];
  for (const sheet of document.styleSheets) {
    ruleList.push(...sheet.cssRules);
  }
  return ruleList;
})();

/**
 * Gets the CSS style by the given selector.
 *
 * @param selector Selector text.
 */
export function cssStyle(selector: string): CSSStyleDeclaration {
  const rule = cssRules.find(
      (rule): rule is CSSStyleRule =>
          rule instanceof CSSStyleRule && rule.selectorText === selector);
  assert(rule !== undefined);
  return assertInstanceof(rule.style, CSSStyleDeclaration);
}
