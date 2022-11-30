// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for mojo bindings file:
 * c/r/r/e/enhanced_network_tts/enhanced_network_tts_custom_bindings.js
 * @externs
 */

/** @constructor */
var EnhancedNetworkTtsAdapter = function() {};

/**
 * @param {!ash.enhancedNetworkTts.mojom.TtsRequest} request
 * @param {function(!ash.enhancedNetworkTts.mojom.TtsResponse)} callback
 * @return {!Promise<void>}
 */
EnhancedNetworkTtsAdapter.prototype.getAudioDataWithCallback = function(
    request, callback) {};

/**  @return {void} */
EnhancedNetworkTtsAdapter.prototype.resetApi = function() {};
