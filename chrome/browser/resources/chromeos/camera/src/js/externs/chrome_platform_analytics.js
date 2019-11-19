// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for chrome-platform-analytics library.
 * @see https://github.com/googlearchive/chrome-platform-analytics/wiki
 * @externs
 */

/* eslint-disable valid-jsdoc */

var analytics = {};

/**
 * @constructor
 * @struct
 */
analytics.EventBuilder = function() {};

/**
 * @param {!analytics.EventBuilder.Dimension} dimension
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.dimension = function(dimension) {};

/**
 * @param {string} category
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.category = function(category) {};

/**
 * @param {string} action
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.action = function(action) {};

/**
 * @param {string} label
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.label = function(label) {};

/**
 * @param {number} value
 * @return {!analytics.EventBuilder}
 */
analytics.EventBuilder.prototype.value = function(value) {};

/**
 * @typedef {{
 *   index: number,
 *   value: string
 * }}
 */
analytics.EventBuilder.Dimension;

/**
 * @interface
 */
analytics.Tracker = function() {};

/**
 * @param {!analytics.EventBuilder} eventBuilder
 */
analytics.Tracker.prototype.send;
