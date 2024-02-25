// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Minimal externs file provided for places in the code that
 * still use JavaScript instead of TypeScript.
 * @externs
 */

/** @interface */
function CrSearchFieldMixinInterface() {}

/**
 * @param {string} value
 * @param {boolean=} noEvent
 */
CrSearchFieldMixinInterface.prototype.setValue = function(value, noEvent) {};

/**
 * @constructor
 * @extends {HTMLElement}
 * @implements {CrSearchFieldMixinInterface}
 */
function CrToolbarSearchFieldElement() {}

/** @return {!HTMLInputElement} */
CrToolbarSearchFieldElement.prototype.getSearchInput = function() {};

CrToolbarSearchFieldElement.prototype.showAndFocus = function() {};

/** @return {boolean} */
CrToolbarSearchFieldElement.prototype.isSearchFocused = function() {};
