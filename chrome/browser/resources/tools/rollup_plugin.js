// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Plugin for rollup to correctly resolve resources.
 */
const path = require('path');

function normalizeSlashes(filepath) {
  return filepath.replace(/\\/gi, '/');
}

function relativePath(from, to) {
  return normalizeSlashes(path.relative(from, to));
}

function joinPaths(a, b) {
  return normalizeSlashes(path.join(a, b));
}

/**
 * Determines the path to |source| from the root directory based on the origin
 * of the request for it. For example, if ../a/b.js is requested from
 * c/d/e/f.js, the returned path will be c/d/a/b.js.
 * @param {string} origin The origin of the request.
 * @param {string} source The requested resource
 * @return {string} Path to source from the root directory.
 */
function combinePaths(origin, source) {
  const originDir = origin ? path.dirname(origin) : '';
  return normalizeSlashes(path.normalize(path.join(originDir, source)));
}

/**
 * @param {string} source Requested resource
 * @param {string} origin The origin of the request
 * @param {string} urlPrefix The URL prefix to check |source| for.
 * @param {string} urlSrcPath The path that corresponds to the URL prefix.
 * @param {!Array<string>} excludes List of paths that should be excluded from
 *     bundling.
 * @return {string} The path to |source|. If |source| does not map to
 *     |urlSrcPath|, returns an empty string. If |source| maps to a location
 *     in |urlSrcPath| but is listed in |excludes|, returns the URL
 *     corresponding to |source|. Otherwise, returns the full path for |source|.
 */
function getPathForUrl(source, origin, urlPrefix, urlSrcPath, excludes) {
  const url = new URL(urlPrefix);
  const schemeRelativeUrl = '//' + url.host + url.pathname;
  let pathFromUrl = '';
  if (source.startsWith(urlPrefix)) {
    pathFromUrl = source.slice(urlPrefix.length);
  } else if (source.startsWith(schemeRelativeUrl)) {
    pathFromUrl = source.slice(schemeRelativeUrl.length);
  } else if (
      !source.includes('://') && !!origin && origin.startsWith(urlSrcPath)) {
    // Relative import from a file that lives in urlSrcPath.
    pathFromUrl = combinePaths(relativePath(urlSrcPath, origin), source);
  }
  if (!pathFromUrl) {
    return '';
  }

  const fullUrl = new URL(pathFromUrl, urlPrefix);
  if (excludes.includes(fullUrl.href)) {
    return fullUrl.href;
  }
  return joinPaths(urlSrcPath, pathFromUrl);
}

export default function plugin(rootPath, hostUrl, excludes, externalPaths) {
  const urlsToPaths = new Map();
  for (const externalPath of externalPaths) {
    const [url, path] = externalPath.split('|', 2);
    urlsToPaths.set(url, path);
  }

  return {
    name: 'webui-path-resolver-plugin',

    resolveId(source, origin) {
      // Normalize origin paths to use forward slashes.
      if (origin) {
        origin = normalizeSlashes(origin);
      }

      for (const [url, path] of urlsToPaths) {
        const resultPath = getPathForUrl(source, origin, url, path, excludes);
        if (resultPath.includes('://')) {
          return {id: resultPath, external: true};
        } else if (resultPath) {
          return resultPath;
        }
      }

      // Not in the URL path map -> should be in the root directory.
      // Check if it should be excluded from the bundle.
      const fullSourcePath = combinePaths(origin, source);
      if (fullSourcePath.startsWith(rootPath)) {
        const pathFromRoot = relativePath(rootPath, fullSourcePath);
        if (excludes.includes(pathFromRoot)) {
          const url = new URL(pathFromRoot, hostUrl);
          return {id: url.href, external: true};
        }
      }

      return null;
    },
  };
}
