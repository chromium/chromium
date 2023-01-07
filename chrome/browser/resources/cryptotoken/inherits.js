// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a partial copy of goog.inherits, so inheritance works
 * even in the absence of Closure.
 */
'use strict';

// A partial copy of goog.inherits, so inheritance works even in the absence of
// Closure.
function inherits(childCtor, parentCtor) {
  /** @constructor */
  function tempCtor() {}
  tempCtor.prototype = parentCtor.prototype;
  childCtor.prototype = new tempCtor();
  childCtor.prototype.constructor = childCtor;
}
