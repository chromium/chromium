// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{hasChildren: boolean,
 *            id: string,
 *            idPath: string,
 *            title: string,
 *            totalUsage: string,
 *            type: string}}
 */
let CookieDetails;

/**
 * @typedef {{content: string,
 *            label: string}}
 */
let CookieDataForDisplay;

// This structure maps the various cookie type names from C++ (hence the
// underscores) to arrays of the different types of data each has, along with
// the i18n name for the description of that data type.
// This structure serves three purposes:
// 1) to list what subset of the cookie data we want to show in the UI.
// 2) What order to show it in.
// 3) What user friendly label to prefix the data with.
const cookieInfo = {
  'cookie': [
    ['name', 'cookieName'], ['content', 'cookieContent'],
    ['domain', 'cookieDomain'], ['path', 'cookiePath'],
    ['sendfor', 'cookieSendFor'],
    ['accessibleToScript', 'cookieAccessibleToScript'],
    ['created', 'cookieCreated'], ['expires', 'cookieExpires']
  ],
  'app_cache': [
    ['origin', 'appCacheOrigin'], ['size', 'localStorageSize'],
    ['modified', 'localStorageLastModified']
  ],
  'database': [
    ['origin', 'databaseOrigin'], ['size', 'localStorageSize'],
    ['modified', 'localStorageLastModified']
  ],
  'local_storage': [
    ['origin', 'localStorageOrigin'], ['size', 'localStorageSize'],
    ['modified', 'localStorageLastModified']
  ],
  'indexed_db': [
    ['origin', 'indexedDbOrigin'], ['size', 'indexedDbSize'],
    ['modified', 'indexedDbLastModified']
  ],
  'file_system': [
    ['origin', 'fileSystemOrigin'], ['persistent', 'fileSystemPersistentUsage'],
    ['temporary', 'fileSystemTemporaryUsage']
  ],
  'service_worker':
      [['origin', 'serviceWorkerOrigin'], ['size', 'serviceWorkerSize']],
  'shared_worker':
      [['worker', 'sharedWorkerWorker'], ['name', 'sharedWorkerName']],
  'cache_storage': [
    ['origin', 'cacheStorageOrigin'], ['size', 'cacheStorageSize'],
    ['modified', 'cacheStorageLastModified']
  ],
  'flash_lso': [['domain', 'cookieDomain']],
  'media_license': [
    ['origin', 'mediaLicenseOrigin'], ['size', 'mediaLicenseSize'],
    ['modified', 'mediaLicenseLastModified']
  ],
};

/**
 * Get cookie data for a given HTML node.
 * @param {CookieDetails} data The contents of the cookie.
 * @return {!Array<CookieDataForDisplay>}
 */
const getCookieData = function(data) {
  /** @type {!Array<CookieDataForDisplay>} */
  const out = [];
  const fields = cookieInfo[data.type];
  for (let i = 0; i < fields.length; i++) {
    const field = fields[i];
    // Iterate through the keys found in |cookieInfo| for the given |type|
    // and see if those keys are present in the data. If so, display them
    // (in the order determined by |cookieInfo|).
    const key = field[0];
    if (data[key].length > 0) {
      const entry = /** @type {CookieDataForDisplay} */ ({
        label: loadTimeData.getString(field[1]),
        content: data[key],
      });
      out.push(entry);
    }
  }
  return out;
};
