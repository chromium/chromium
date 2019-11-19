// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';


// Closure compiler won't let this be declared inside cr.define().
/** @enum {string} */
export const SourceType = {
  WEBSTORE: 'webstore',
  POLICY: 'policy',
  SIDELOADED: 'sideloaded',
  UNPACKED: 'unpacked',
  UNKNOWN: 'unknown',
};

/**
 * Returns true if the extension is enabled, including terminated
 * extensions.
 * @param {!chrome.developerPrivate.ExtensionState} state
 * @return {boolean}
 */
export function isEnabled(state) {
  switch (state) {
    case chrome.developerPrivate.ExtensionState.ENABLED:
    case chrome.developerPrivate.ExtensionState.TERMINATED:
      return true;
    case chrome.developerPrivate.ExtensionState.BLACKLISTED:
    case chrome.developerPrivate.ExtensionState.DISABLED:
      return false;
  }
  assertNotReached();
}

/**
 * @param {!chrome.developerPrivate.ExtensionInfo} extensionInfo
 * @return {boolean} Whether the extension is controlled.
 */
export function isControlled(extensionInfo) {
  return !!extensionInfo.controlledInfo;
}

/**
 * Returns true if the user can change whether or not the extension is
 * enabled.
 * @param {!chrome.developerPrivate.ExtensionInfo} item
 * @return {boolean}
 */
export function userCanChangeEnablement(item) {
  // User doesn't have permission.
  if (!item.userMayModify) {
    return false;
  }
  // Item is forcefully disabled.
  if (item.disableReasons.corruptInstall ||
      item.disableReasons.suspiciousInstall ||
      item.disableReasons.updateRequired) {
    return false;
  }
  // An item with dependent extensions can't be disabled (it would bork the
  // dependents).
  if (item.dependentExtensions.length > 0) {
    return false;
  }
  // Blacklisted can't be enabled, either.
  if (item.state == chrome.developerPrivate.ExtensionState.BLACKLISTED) {
    return false;
  }

  return true;
}

/**
 * @param {!chrome.developerPrivate.ExtensionInfo} item
 * @return {SourceType}
 */
export function getItemSource(item) {
  if (item.controlledInfo &&
      item.controlledInfo.type ==
          chrome.developerPrivate.ControllerType.POLICY) {
    return SourceType.POLICY;
  }

  switch (item.location) {
    case chrome.developerPrivate.Location.THIRD_PARTY:
      return SourceType.SIDELOADED;
    case chrome.developerPrivate.Location.UNPACKED:
      return SourceType.UNPACKED;
    case chrome.developerPrivate.Location.UNKNOWN:
      return SourceType.UNKNOWN;
    case chrome.developerPrivate.Location.FROM_STORE:
      return SourceType.WEBSTORE;
  }

  assertNotReached(item.location);
}

/**
 * @param {SourceType} source
 * @return {string}
 */
export function getItemSourceString(source) {
  switch (source) {
    case SourceType.POLICY:
      return loadTimeData.getString('itemSourcePolicy');
    case SourceType.SIDELOADED:
      return loadTimeData.getString('itemSourceSideloaded');
    case SourceType.UNPACKED:
      return loadTimeData.getString('itemSourceUnpacked');
    case SourceType.WEBSTORE:
      return loadTimeData.getString('itemSourceWebstore');
    case SourceType.UNKNOWN:
      // Nothing to return. Calling code should use
      // chrome.developerPrivate.ExtensionInfo's |locationText| instead.
      return '';
  }
  assertNotReached();
}

/**
 * Computes the human-facing label for the given inspectable view.
 * @param {!chrome.developerPrivate.ExtensionView} view
 * @return {string}
 */
export function computeInspectableViewLabel(view) {
  // Trim the "chrome-extension://<id>/".
  const url = new URL(view.url);
  let label = view.url;
  if (url.protocol == 'chrome-extension:') {
    label = url.pathname.substring(1);
  }
  if (label == '_generated_background_page.html') {
    label = loadTimeData.getString('viewBackgroundPage');
  }
  // Add any qualifiers.
  if (view.incognito) {
    label += ' ' + loadTimeData.getString('viewIncognito');
  }
  if (view.renderProcessId == -1) {
    label += ' ' + loadTimeData.getString('viewInactive');
  }
  if (view.isIframe) {
    label += ' ' + loadTimeData.getString('viewIframe');
  }

  return label;
}
