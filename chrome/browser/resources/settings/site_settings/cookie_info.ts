// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

export interface CookieDetails {
  hasChildren: boolean;
  id: string;
  idPath: string;
  title: string;
  totalUsage: string;
  type: string;
}

export interface CookieDataForDisplay {
  content: string;
  label: string;
}

// This structure maps the various cookie type names from C++ (hence the
// underscores) to arrays of the different types of data each has, along with
// the i18n name for the description of that data type.
// This structure serves three purposes:
// 1) to list what subset of the cookie data we want to show in the UI.
// 2) What order to show it in.
// 3) What user friendly label to prefix the data with.
export const cookieInfo: {[key: string]: string[][]} = {
  'cookie': [
    ['name', 'cookieName'],
    ['content', 'cookieContent'],
    ['domain', 'cookieDomain'],
    ['path', 'cookiePath'],
    ['sendfor', 'cookieSendFor'],
    ['accessibleToScript', 'cookieAccessibleToScript'],
    ['created', 'cookieCreated'],
    ['expires', 'cookieExpires'],
  ],
  'database': [
    ['origin', 'databaseOrigin'],
    ['size', 'localStorageSize'],
    ['modified', 'localStorageLastModified'],
  ],
  'local_storage': [
    ['origin', 'localStorageOrigin'],
    ['size', 'localStorageSize'],
    ['modified', 'localStorageLastModified'],
  ],
  'indexed_db': [
    ['origin', 'indexedDbOrigin'],
    ['size', 'indexedDbSize'],
    ['modified', 'indexedDbLastModified'],
  ],
  'file_system': [
    ['origin', 'fileSystemOrigin'],
    ['persistent', 'fileSystemPersistentUsage'],
    ['temporary', 'fileSystemTemporaryUsage'],
  ],
  'quota': [['origin', 'quotaOrigin'], ['totalUsage', 'quotaSize']],
  'service_worker':
      [['origin', 'serviceWorkerOrigin'], ['size', 'serviceWorkerSize']],
  'shared_worker':
      [['worker', 'sharedWorkerWorker'], ['name', 'sharedWorkerName']],
  'cache_storage': [
    ['origin', 'cacheStorageOrigin'],
    ['size', 'cacheStorageSize'],
    ['modified', 'cacheStorageLastModified'],
  ],
  'flash_lso': [['domain', 'cookieDomain']],
};

/**
 * Get cookie data for a given HTML node.
 * @param data The contents of the cookie.
 */
export function getCookieData(data: CookieDetails): CookieDataForDisplay[] {
  const out: CookieDataForDisplay[] = [];
  const fields = cookieInfo[data.type];
  for (let i = 0; i < fields.length; i++) {
    const field = fields[i];
    // Iterate through the keys found in |cookieInfo| for the given |type|
    // and see if those keys are present in the data. If so, display them
    // (in the order determined by |cookieInfo|).
    const key = field[0];
    const value = (data as unknown as {[key: string]: string})[key];
    if (value.length > 0) {
      const entry = {
        label: loadTimeData.getString(field[1]),
        content: value,
      };
      out.push(entry);
    }
  }
  return out;
}
