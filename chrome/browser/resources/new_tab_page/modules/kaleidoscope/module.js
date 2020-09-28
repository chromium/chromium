// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/origin.mojom-lite.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {ModuleDescriptor} from '../module_descriptor.js';

/**
 * @typedef {{
 *   url: (string),
 *   module: (?boolean)
 * }}
 */
const Resource = {};

/**
 * TODO(beccahughes): use import for these.
 * @type {Array<Resource>}
 */
const KALEIDOSCOPE_RESOURCES = [
  {url: 'chrome://kaleidoscope/geometry.mojom-lite.js'},
  {
    url:
        'chrome://kaleidoscope/chrome/browser/media/feeds/media_feeds_store.mojom-lite.js'
  },
  {url: 'chrome://kaleidoscope/kaleidoscope.mojom-lite.js'},
  {url: 'chrome://kaleidoscope/content.js'},
  {url: 'chrome://kaleidoscope/resources/_locales/strings.js'},
  {url: 'chrome://kaleidoscope/module.js', module: true},
];

/**
 * Loads a script resource and returns a promise that will resolve when the
 * loading is complete.
 * @param {Resource} resource
 * @returns {Promise}
 */
function loadResource(resource) {
  return new Promise((resolve) => {
    const script = document.createElement('script');

    if (resource.module) {
      script.type = 'module';
    }

    script.src = resource.url;
    script.addEventListener('load', resolve, {once: true});
    document.body.appendChild(script);
  });
}

/** @type {!ModuleDescriptor} */
export const kaleidoscopeDescriptor = new ModuleDescriptor(
    /*id=*/ 'kaleidoscope',
    /*heightPx=*/ 330,
    async () => {
      // Load all the Kaleidoscope resources into the NTP and return the module
      // once the loading is complete.
      await Promise.all(KALEIDOSCOPE_RESOURCES.map((r) => loadResource(r)));

      return window.loadKaleidoscopeModule();
    },
);
