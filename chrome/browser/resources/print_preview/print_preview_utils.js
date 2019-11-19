// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

import {isRTL} from 'chrome://resources/js/util.m.js';

import {DarkModeBehavior} from './dark_mode_behavior.js';

/**
 * Returns true if the contents of the two page ranges are equal.
 * @param {!Array<{ to: number, from: number }>} array1 The first array.
 * @param {!Array<{ to: number, from: number }>} array2 The second array.
 * @return {boolean} true if the arrays are equal.
 */
export function areRangesEqual(array1, array2) {
  if (array1.length != array2.length) {
    return false;
  }
  for (let i = 0; i < array1.length; i++) {
    if (array1[i].from != array2[i].from || array1[i].to != array2[i].to) {
      return false;
    }
  }
  return true;
}

/**
 * @param {!Array<!{locale: string, value: string}>} localizedStrings An array
 *     of strings with corresponding locales.
 * @param {string} locale Locale to look the string up for.
 * @return {string} A string for the requested {@code locale}. An empty string
 *     if there's no string for the specified locale found.
 */
export function getStringForLocale(localizedStrings, locale) {
  locale = locale.toLowerCase();
  for (let i = 0; i < localizedStrings.length; i++) {
    if (localizedStrings[i].locale.toLowerCase() == locale) {
      return localizedStrings[i].value;
    }
  }
  return '';
}

/**
 * @param {!Array<!{locale: string, value: string}>} localizedStrings An array
 *     of strings with corresponding locales.
 * @return {string} A string for the current locale. An empty string if there's
 *     no string for the current locale found.
 */
export function getStringForCurrentLocale(localizedStrings) {
  // First try to find an exact match and then look for the language only.
  return getStringForLocale(localizedStrings, navigator.language) ||
      getStringForLocale(localizedStrings, navigator.language.split('-')[0]);
}

/**
 * @param {!Array<*>} args The arguments for the observer.
 * @return {boolean} Whether all arguments are defined.
 */
export function observerDepsDefined(args) {
  return args.every(arg => arg !== undefined);
}

/**
 * Returns background images (icon and dropdown arrow) for use in a md-select.
 * @param {!IronIconsetSvgElement} iconset The iconset the icon is in.
 * @param {string} iconName The icon name
 * @param {!HTMLElement} el The element that contains the select.
 * @return {string} String containing inlined SVG of the icon and
 *     url(path_to_arrow) separated by a comma.
 */
export function getSelectDropdownBackground(iconset, iconName, el) {
  const serializer = new XMLSerializer();
  const iconElement = iconset.createIcon(iconName, isRTL());
  const inDarkMode = DarkModeBehavior.inDarkMode();
  const fillColor = getComputedStyle(el).getPropertyValue(
      inDarkMode ? '--google-grey-refresh-500' : '--google-grey-600');
  iconElement.style.fill = fillColor;
  const serializedIcon = serializer.serializeToString(iconElement);
  const uri = encodeURIComponent(serializedIcon);
  const arrowDownPath = inDarkMode ?
      'chrome://resources/images/dark/arrow_down.svg' :
      'chrome://resources/images/arrow_down.svg';
  return `url("data:image/svg+xml;charset=utf-8,${uri}"),` +
      `url("${arrowDownPath}")`;
}
