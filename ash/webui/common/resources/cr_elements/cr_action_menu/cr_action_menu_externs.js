// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/**
 * @typedef {{
 *   top: (number|undefined),
 *   left: (number|undefined),
 *   width: (number|undefined),
 *   height: (number|undefined),
 *   anchorAlignmentX: (number|undefined),
 *   anchorAlignmentY: (number|undefined),
 *   minX: (number|undefined),
 *   minY: (number|undefined),
 *   maxX: (number|undefined),
 *   maxY: (number|undefined),
 *   noOffset: (boolean|undefined),
 * }}
 */
let ShowAtConfig;

/**
 * @constructor
 * @extends {HTMLElement}
 */
function CrActionMenuElement() {}

/** @type {boolean} */
CrActionMenuElement.prototype.open;

/** @return {!HTMLDialogElement} */
CrActionMenuElement.prototype.getDialog = function() {};

/**
 * @param {!HTMLElement} anchorElement
 * @param {ShowAtConfig=} config
 */
CrActionMenuElement.prototype.showAt = function(anchorElement, config) {};

CrActionMenuElement.prototype.close = function() {};
