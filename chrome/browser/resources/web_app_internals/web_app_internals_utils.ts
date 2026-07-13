// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents the top-level debug info JSON output emitted by
 * BuildDebugInfo() in web_app_internals_handler.cc.
 */
export interface DebugData {
  // Always emitted.
  InstalledWebApps: InstalledWebAppsData;
  LockManager: Record<string, unknown>;
  NavigationCapturing: unknown[];
  CommandManager: Record<string, unknown>;
  IconErrorLog: string[]|string;
  PreinstalledWebAppConfigs: Record<string, unknown>|string;
  UserUninstalledPreinstalledWebAppPrefs: Record<string, unknown>;
  WebAppPreferences: Record<string, unknown>;
  WebAppIphPreferences: Record<string, unknown>;
  WebAppMlPreferences: Record<string, unknown>;
  WebAppIPHLinkCapturingPreferences: Record<string, unknown>;
  ShouldGarbageCollectStoragePartitions: boolean;
  IsolatedWebAppUpdateManager: Record<string, unknown>;
  IsolatedWebAppPolicyManager: Record<string, unknown>;
  IwaKeyDistributionInfoProvider: Record<string, unknown>;
  WebAppDirectoryDiskState: Record<string, unknown>;

  // Conditionally emitted.
  AppShimRegistryLocalStorage?: Record<string, unknown>;  // Mac only.
  IwaBundleCacheManager?: Record<string, unknown>;        // ChromeOS only.
  DatabaseLog?: unknown[];  // Absent when debug recording is disabled.
}

/**
 * Canonical ordering of top-level DebugData keys matching the logical subsystem
 * grouping rather than alphabetical order, improving scannability and reading.
 */
const DebugDataKeyOrder: ReadonlyArray<keyof DebugData> = [
  'InstalledWebApps',
  'AppShimRegistryLocalStorage',
  'LockManager',
  'NavigationCapturing',
  'CommandManager',
  'DatabaseLog',
  'IconErrorLog',
  'PreinstalledWebAppConfigs',
  'UserUninstalledPreinstalledWebAppPrefs',
  'WebAppPreferences',
  'WebAppIphPreferences',
  'WebAppMlPreferences',
  'WebAppIPHLinkCapturingPreferences',
  'ShouldGarbageCollectStoragePartitions',
  'IsolatedWebAppUpdateManager',
  'IsolatedWebAppPolicyManager',
  'IwaBundleCacheManager',
  'IwaKeyDistributionInfoProvider',
  'WebAppDirectoryDiskState',
];

/**
 * Result of filterToApp(): either the full DebugData (when filtering doesn't
 * apply) or a subset containing only the filtered InstalledWebApps section.
 */
export type FilteredDebugData = DebugData|Pick<DebugData, 'InstalledWebApps'>;

/**
 * Typed representation of the InstalledWebApps section produced by
 * WebAppRegistrar::AsDebugValue().
 */
export interface InstalledWebAppsData {
  '!Index': Record<string, string|string[]>;
  'Details': Array<{'!app_id': string, [key: string]: unknown}>;
}

/**
 * Represents a single navigation link entry in the installed web apps index.
 */
export interface AppIndexEntry {
  id: string;  // Empty string '' represents the 'Show All' entry.
  label: string;
  isActive: boolean;
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
 * Extracts and formats the index entries of installed web apps from the parsed
 * debug JSON for declarative rendering in Lit.
 */
export function getAppIndexEntries(
    data: DebugData, query: string): AppIndexEntry[] {
  const entries: AppIndexEntry[] = [];
  const appIndex = data.InstalledWebApps['!Index'];

  let queryMatchFound = false;
  for (const [name, idOrIds] of Object.entries(appIndex)) {
    const ids: string[] = Array.isArray(idOrIds) ? idOrIds : [idOrIds];
    for (const id of ids) {
      const isActive = (id === query);
      if (isActive) {
        queryMatchFound = true;
      }
      entries.push({
        id,
        label: `${name} (${id})`,
        isActive,
      });
    }
  }

  // Prepend the 'Show All' navigation entry.
  entries.unshift({
    id: '',
    label: 'Show All',
    isActive: !query || !queryMatchFound,
  });

  return entries;
}

/**
 * Builds a clickable index of installed web apps from the parsed debug JSON.
 * Each app name becomes a link that filters the page to that app's details.
 * The currently selected app (based on URL hash) is highlighted.
 */
export function renderAppIndex(
    data: DebugData, indexContainer: HTMLElement, query: string): void {
  indexContainer.replaceChildren();

  for (const entry of getAppIndexEntries(data, query)) {
    const link = document.createElement('a');
    link.href = `#${entry.id || ''}`;
    link.textContent = entry.label;
    link.classList.toggle('active', entry.isActive);
    indexContainer.appendChild(link);
  }
}

/**
 * Filters the debug data to only show the InstalledWebApps section with the
 * matching app's details. The !Index is preserved for navigation. All other
 * sections are removed to reduce clutter.
 */
export function filterToApp(data: DebugData, appId: string): FilteredDebugData {
  const installedWebApps = data.InstalledWebApps;

  const details = installedWebApps['Details'];
  const filtered = details.filter(app => app['!app_id'] === appId);
  if (filtered.length === 0) {
    return data;
  }
  return {
    InstalledWebApps: {
      ...installedWebApps,
      Details: filtered,
    },
  };
}

/**
 * A JSON.stringify replacer function (second parameter) that orders top-level
 * DebugData dictionary keys into a logical subsystem sequence matching
 * DebugDataKeyOrder while preserving all nested properties and sections intact.
 */
export function debugDataJsonReplacer(key: string, value: unknown): unknown {
  if (key === '' && value && typeof value === 'object' &&
      !Array.isArray(value)) {
    const ordered: Record<string, unknown> = {};
    const dict = value as Record<string, unknown>;

    for (const k of DebugDataKeyOrder) {
      if (k in dict) {
        ordered[k] = dict[k];
      }
    }

    for (const [k, v] of Object.entries(dict)) {
      if (!(k in ordered) && v !== undefined) {
        ordered[k] = v;
      }
    }

    return ordered;
  }

  return value;
}
