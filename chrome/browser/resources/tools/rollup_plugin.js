// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Plugin for rollup to correctly resolve resources.
 */
const path = require('path');

const chromeResourcesUrl = 'chrome://resources/';
const polymerUrl = 'chrome://resources/polymer/v3_0/';

// TODO: Determine whether it is worth maintaining this list vs always checking
// both directories for the existence of a file.
const nonGeneratedFiles = ['cr.m.js', 'action_link.js'];

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

export default function plugin(srcPath, genPath, rootPath, host, excludes) {
  const resourcesSrcPath = joinPaths(srcPath, 'ui/webui/resources/');
  const polymerSrcPath =
      joinPaths(srcPath, 'third_party/polymer/v3_0/components-chromium/');
  const resourcesGenPath = joinPaths(genPath, 'ui/webui/resources/');
  const rootUrl = 'chrome://' + host + '/';

  return {
    name: 'webui-path-resolver-plugin',

    resolveId(source, origin) {
      // Normalize origin paths to use forward slashes.
      if (origin) {
        origin = normalizeSlashes(origin);
      }

      // Handle polymer resources
      let pathFromPolymer = '';
      if (source.startsWith(polymerUrl)) {
        pathFromPolymer = source.slice(polymerUrl.length);
      } else if (!!origin && origin.startsWith(polymerSrcPath)) {
        pathFromPolymer =
            combinePaths(relativePath(polymerSrcPath, origin), source);
      }
      if (pathFromPolymer) {
        const fullPath = polymerUrl + pathFromPolymer;
        if (excludes.includes(fullPath)) {
          return {id: fullPath, external: true};
        }
        return joinPaths(polymerSrcPath, pathFromPolymer);
      }

      // Get path from ui/webui/resources
      let pathFromResources = '';
      if (source.startsWith(chromeResourcesUrl)) {
        pathFromResources = source.slice(chromeResourcesUrl.length);
      } else if (!!origin && origin.startsWith(resourcesSrcPath)) {
        pathFromResources =
            combinePaths(relativePath(resourcesSrcPath, origin), source);
      } else if (!!origin && origin.startsWith(resourcesGenPath)) {
        pathFromResources =
            combinePaths(relativePath(resourcesGenPath, origin), source);
      }

      // Add prefix
      if (pathFromResources) {
        const fullPath = chromeResourcesUrl + pathFromResources;
        if (excludes.includes(fullPath)) {
          return {id: fullPath, external: true};
        }
        const filename = path.basename(source);
        return joinPaths(
            nonGeneratedFiles.includes(filename) ? resourcesSrcPath :
                                                   resourcesGenPath,
            pathFromResources);
      }

      // Not a resources or polymer path -> should be in the root directory.
      // Check if it should be excluded from the bundle.
      const fullSourcePath = combinePaths(origin, source);
      if (fullSourcePath.startsWith(rootPath)) {
        const pathFromRoot = relativePath(rootPath, fullSourcePath);
        if (excludes.includes(pathFromRoot)) {
          return {id: rootUrl + pathFromRoot, external: true};
        }
      }

      return null;
    },
  };
}
