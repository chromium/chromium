// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/js/util_ts.js';
import {IronIconsetSvgElement} from 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

import {inDarkMode} from './dark_mode_mixin.js';
import {LocalizedString} from './data/cdd.js';

export interface Range {
  to: number;
  from: number;
}

/**
 * Returns true if the contents of the two page ranges are equal.
 */
export function areRangesEqual(array1: Range[], array2: Range[]): boolean {
  if (array1.length !== array2.length) {
    return false;
  }
  for (let i = 0; i < array1.length; i++) {
    if (array1[i].from !== array2[i].from || array1[i].to !== array2[i].to) {
      return false;
    }
  }
  return true;
}

/**
 * @param localizedStrings An array of strings with corresponding locales.
 * @param locale Locale to look the string up for.
 * @return A string for the requested {@code locale}. An empty string
 *     if there's no string for the specified locale found.
 */
function getStringForLocale(
    localizedStrings: LocalizedString[], locale: string): string {
  locale = locale.toLowerCase();
  for (let i = 0; i < localizedStrings.length; i++) {
    if (localizedStrings[i].locale.toLowerCase() === locale) {
      return localizedStrings[i].value;
    }
  }
  return '';
}

/**
 * @param localizedStrings An array of strings with corresponding locales.
 * @return A string for the current locale. An empty string if there's
 *     no string for the current locale found.
 */
export function getStringForCurrentLocale(localizedStrings: LocalizedString[]):
    string {
  // First try to find an exact match and then look for the language only.
  return getStringForLocale(localizedStrings, navigator.language) ||
      getStringForLocale(localizedStrings, navigator.language.split('-')[0]);
}

/**
 * @param args The arguments for the observer.
 * @return Whether all arguments are defined.
 */
export function observerDepsDefined(args: any[]): boolean {
  return args.every(arg => arg !== undefined);
}

/**
 * Returns background images (icon and dropdown arrow) for use in a md-select.
 * @param iconset The iconset the icon is in.
 * @param iconName The icon name
 * @param el The element that contains the select.
 * @return String containing inlined SVG of the icon and
 *     url(path_to_arrow) separated by a comma.
 */
export function getSelectDropdownBackground(
    iconset: IronIconsetSvgElement, iconName: string, el: HTMLElement): string {
  const serializer = new XMLSerializer();
  const iconElement = iconset.createIcon(iconName, isRTL()) as HTMLElement;
  const dark = inDarkMode();
  const fillColor = getComputedStyle(el).getPropertyValue(
      dark ? '--google-grey-500' : '--google-grey-600');
  iconElement.style.fill = fillColor;
  const serializedIcon = serializer.serializeToString(iconElement);
  const uri = encodeURIComponent(serializedIcon);
  const arrowDownPath = dark ? 'chrome://resources/images/dark/arrow_down.svg' :
                               'chrome://resources/images/arrow_down.svg';
  return `url("data:image/svg+xml;charset=utf-8,${uri}"),` +
      `url("${arrowDownPath}")`;
}
