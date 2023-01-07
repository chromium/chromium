// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Provides a shim to allow select-to-speak to use closure files.
 */

var goog = {};

goog.addDependency = function() {};

goog.provide = function(n) {
  globalThis[n] = {};
};

goog.require = function() {};

goog.scope = function(c) {
  c();
};
