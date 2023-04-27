// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {installLaunchHandler} from './launch.js';

/**
 * Gets the query string from the URL.
 * For example, if the URL is chrome://projector/annotator/abc?resourceKey=xyz,
 * then query is "abc?resourceKey=xyz".
 */
function getQuery() {
  if (!document.location.href) {
    return '';
  }
  const paths = document.location.href.split('/');
  if (paths.length < 1) {
    return '';
  }
  return paths[paths.length - 1];
}

Polymer({
  is: 'app-embedder',

  behaviors: [],

  /** @override */
  ready() {
    document.body.querySelector('iframe').src =
        'chrome-untrusted://projector/' + getQuery();
  },
});

installLaunchHandler();
