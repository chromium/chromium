// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents a top-level section in the debug info JSON output.
 * Each section is a single-key object mapping a section name to its data.
 */
export interface DebugSection {
  [key: string]: unknown;
}

/**
 * Typed representation of the InstalledWebApps section produced by
 * WebAppRegistrar::AsDebugValue().
 */
export interface InstalledWebAppsData {
  '!Index': Record<string, string|string[]>;
  'Details': Array<{'!app_id': string, [key: string]: unknown}>;
}

/**
 * Gets the app ID query from the URL hash fragment.
 * For example, if the URL is "chrome://web-app-internals/#abc" then the
 * query is "abc".
 */
export function getQuery(): string {
  if (document.location.hash) {
    return document.location.hash.substring(1);
  }
  return '';
}

/**
 * Type guard that identifies a section containing InstalledWebApps data.
 * The value shape is guaranteed by WebAppRegistrar::AsDebugValue().
 */
function hasInstalledWebApps(section: DebugSection):
    section is {InstalledWebApps: InstalledWebAppsData} {
  return Object.prototype.hasOwnProperty.call(section, 'InstalledWebApps');
}

/**
 * Extracts the InstalledWebApps record from the debug data sections.
 */
function getInstalledWebApps(data: DebugSection[]): InstalledWebAppsData|
    undefined {
  const section = data.find(hasInstalledWebApps);
  if (!section) {
    return undefined;
  }
  return section.InstalledWebApps;
}

/**
 * Builds a clickable index of installed web apps from the parsed debug JSON.
 * Each app name becomes a link that filters the page to that app's details.
 * The currently selected app (based on URL hash) is highlighted.
 */
export function renderAppIndex(
    data: DebugSection[], indexContainer: HTMLElement, query: string): void {
  indexContainer.replaceChildren();

  const installedWebApps = getInstalledWebApps(data);
  if (!installedWebApps) {
    return;
  }

  const appIndex = installedWebApps['!Index'];

  const showAllLink = document.createElement('a');
  showAllLink.href = '#';
  showAllLink.textContent = 'Show All';
  indexContainer.appendChild(showAllLink);

  let queryMatchFound = false;
  for (const [name, idOrIds] of Object.entries(appIndex)) {
    const ids: string[] = Array.isArray(idOrIds) ? idOrIds : [idOrIds];
    for (const id of ids) {
      const link = document.createElement('a');
      link.href = '#' + id;
      link.textContent = `${name} (${id})`;
      if (id === query) {
        link.classList.add('active');
        queryMatchFound = true;
      }
      indexContainer.appendChild(link);
    }
  }

  if (!query || !queryMatchFound) {
    showAllLink.classList.add('active');
  }
}

/**
 * Filters the debug data to only show the InstalledWebApps section with the
 * matching app's details. The !Index is preserved for navigation. All other
 * sections are removed to reduce clutter.
 */
export function filterToApp(
    data: DebugSection[], appId: string): DebugSection[] {
  const installedWebApps = getInstalledWebApps(data);
  if (!installedWebApps) {
    return data;
  }

  const details = installedWebApps['Details'];
  const filtered = details.filter(app => app['!app_id'] === appId);
  if (filtered.length === 0) {
    return data;
  }
  return [{
    'InstalledWebApps': {
      ...installedWebApps,
      'Details': filtered,
    },
  }];
}
