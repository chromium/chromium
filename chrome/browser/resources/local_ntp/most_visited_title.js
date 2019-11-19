// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Rendering for iframed most visited titles.
 */

window.addEventListener('DOMContentLoaded', function() {
  'use strict';

  fillMostVisited(window.location, function(params, data) {
    document.body.appendChild(createMostVisitedLink(
        params, data.url, data.title, data.title, data.direction));
  });
});
