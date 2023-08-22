// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

export enum SourceType {
  WEBSTORE = 'webstore',
  POLICY = 'policy',
  SIDELOADED = 'sideloaded',
  UNPACKED = 'unpacked',
  INSTALLED_BY_DEFAULT = 'installed-by-default',
  UNKNOWN = 'unknown',
}

export enum EnableControl {
  RELOAD = 'RELOAD',
  REPAIR = 'REPAIR',
  ENABLE_TOGGLE = 'ENABLE_TOGGLE',
}

// TODO(tjudkins): This should be extracted to a shared metrics module.
export enum UserAction {
  ALL_TOGGLED_ON = 'Extensions.Settings.HostList.AllHostsToggledOn',
  ALL_TOGGLED_OFF = 'Extensions.Settings.HostList.AllHostsToggledOff',
  SPECIFIC_TOGGLED_ON = 'Extensions.Settings.HostList.SpecificHostToggledOn',
  SPECIFIC_TOGGLED_OFF = 'Extensions.Settings.HostList.SpecificHostToggledOff',
  LEARN_MORE = 'Extensions.Settings.HostList.LearnMoreActivated',
}

/**
 * Returns true if the extension is enabled, including terminated
 * extensions.
 */
export function isEnabled(state: chrome.developerPrivate.ExtensionState):
    boolean {
  switch (state) {
    case chrome.developerPrivate.ExtensionState.ENABLED:
    case chrome.developerPrivate.ExtensionState.TERMINATED:
      return true;
    case chrome.developerPrivate.ExtensionState.BLACKLISTED:
    case chrome.developerPrivate.ExtensionState.DISABLED:
      return false;
    default:
      assertNotReached();
  }
}

/**
 * @return Whether the user can change whether or not the extension is
 *     enabled.
 */
export function userCanChangeEnablement(
    item: chrome.developerPrivate.ExtensionInfo): boolean {
  // User doesn't have permission.
  if (!item.userMayModify) {
    return false;
  }
  // Item is forcefully disabled.
  if (item.disableReasons.corruptInstall ||
      item.disableReasons.suspiciousInstall ||
      item.disableReasons.updateRequired ||
      item.disableReasons.publishedInStoreRequired ||
      item.disableReasons.blockedByPolicy) {
    return false;
  }
  // An item with dependent extensions can't be disabled (it would bork the
  // dependents).
  if (item.dependentExtensions.length > 0) {
    return false;
  }
  // Blacklisted can't be enabled, either.
  if (item.state === chrome.developerPrivate.ExtensionState.BLACKLISTED) {
    return false;
  }

  return true;
}

export function getItemSource(item: chrome.developerPrivate.ExtensionInfo):
    SourceType {
  if (item.controlledInfo) {
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
    case chrome.developerPrivate.Location.INSTALLED_BY_DEFAULT:
      return SourceType.INSTALLED_BY_DEFAULT;
    default:
      assertNotReached(item.location);
  }
}

export function getItemSourceString(source: SourceType): string {
  switch (source) {
    case SourceType.POLICY:
      return loadTimeData.getString('itemSourcePolicy');
    case SourceType.SIDELOADED:
      return loadTimeData.getString('itemSourceSideloaded');
    case SourceType.UNPACKED:
      return loadTimeData.getString('itemSourceUnpacked');
    case SourceType.WEBSTORE:
      return loadTimeData.getString('itemSourceWebstore');
    case SourceType.INSTALLED_BY_DEFAULT:
      return loadTimeData.getString('itemSourceInstalledByDefault');
    case SourceType.UNKNOWN:
      // Nothing to return. Calling code should use
      // chrome.developerPrivate.ExtensionInfo's |locationText| instead.
      return '';
    default:
      assertNotReached();
  }
}

/**
 * Computes the human-facing label for the given inspectable view.
 */
export function computeInspectableViewLabel(
    view: chrome.developerPrivate.ExtensionView): string {
  // Trim the "chrome-extension://<id>/".
  const url = new URL(view.url);
  let label = view.url;
  if (url.protocol === 'chrome-extension:') {
    label = url.pathname.substring(1);
  }
  if (label === '_generated_background_page.html') {
    label = loadTimeData.getString('viewBackgroundPage');
  }
  if (view.type === 'EXTENSION_SERVICE_WORKER_BACKGROUND') {
    label = loadTimeData.getString('viewServiceWorker');
  }
  // Add any qualifiers.
  if (view.incognito) {
    label += ' ' + loadTimeData.getString('viewIncognito');
  }
  if (view.renderProcessId === -1) {
    label += ' ' + loadTimeData.getString('viewInactive');
  }
  if (view.isIframe) {
    label += ' ' + loadTimeData.getString('viewIframe');
  }

  return label;
}

/**
 * Computes the accessible human-facing aria label for an extension toggle item.
 */
export function getEnableToggleAriaLabel(
    toggleEnabled: boolean,
    extensionsDataType: chrome.developerPrivate.ExtensionType,
    appEnabled: string, extensionEnabled: string, itemOff: string): string {
  if (!toggleEnabled) {
    return itemOff;
  }

  const ExtensionType = chrome.developerPrivate.ExtensionType;
  switch (extensionsDataType) {
    case ExtensionType.HOSTED_APP:
    case ExtensionType.LEGACY_PACKAGED_APP:
    case ExtensionType.PLATFORM_APP:
      return appEnabled;
    case ExtensionType.EXTENSION:
    case ExtensionType.SHARED_MODULE:
      return extensionEnabled;
  }
  assertNotReached('Item type is not App or Extension.');
}

/**
 * Clones the array and returns a new array with background pages and service
 * workers sorted before other views.
 * @returns Sorted array.
 */
export function sortViews(views: chrome.developerPrivate.ExtensionView[]):
    chrome.developerPrivate.ExtensionView[] {
  function getSortValue(view: chrome.developerPrivate.ExtensionView): number {
    switch (view.type) {
      case chrome.developerPrivate.ViewType.EXTENSION_SERVICE_WORKER_BACKGROUND:
        return 2;
      case chrome.developerPrivate.ViewType.EXTENSION_BACKGROUND_PAGE:
        return 1;
      default:
        return 0;
    }
  }

  return [...views].sort((a, b) => getSortValue(b) - getSortValue(a));
}

/**
 * @return Whether the extension is in the terminated state.
 */
function isTerminated(state: chrome.developerPrivate.ExtensionState): boolean {
  return state === chrome.developerPrivate.ExtensionState.TERMINATED;
}

/**
 * Determines which enable control to display for a given extension.
 */
export function getEnableControl(data: chrome.developerPrivate.ExtensionInfo):
    EnableControl {
  if (isTerminated(data.state)) {
    return EnableControl.RELOAD;
  }
  if (data.disableReasons.corruptInstall && data.userMayModify) {
    return EnableControl.REPAIR;
  }
  return EnableControl.ENABLE_TOGGLE;
}

/**
 * @return The tooltip to show for an extension's enable toggle.
 */
export function getEnableToggleTooltipText(
    data: chrome.developerPrivate.ExtensionInfo): string {
  if (!isEnabled(data.state)) {
    return loadTimeData.getString('enableToggleTooltipDisabled');
  }

  return loadTimeData.getString(
      data.permissions.canAccessSiteData ?
          'enableToggleTooltipEnabledWithSiteAccess' :
          'enableToggleTooltipEnabled');
}
