// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EnhancedNetworkTts} from './enhanced_network_tts.js';

/**
 * Register the listener for onSpeakWithAudioStream event. The event will be
 * called when the user makes a call to tts.speak() and one of the voices from
 * this extension's manifest is the first to match the options object.
 */
chrome.ttsEngine.onSpeakWithAudioStream.addListener(
    async (
        /** string */ utterance,
        /** !chrome.ttsEngine.SpeakOptions */ options,
        /** !chrome.ttsEngine.AudioStreamOptions */ audioStreamOptions,
        /** function(!chrome.ttsEngine.AudioBuffer): void */ sendTtsAudio,
        /** function(string): void */ sendError) =>
        await EnhancedNetworkTts.onSpeakWithAudioStreamEvent(
            utterance, options, audioStreamOptions, sendTtsAudio, sendError));

// The onStop listener is needed for the |tts_engine_events::kOnStop| check in
// tts_engine_extension_api.cc
// TODO(crbug.com/1231318): Clear or cancel the current network request.
chrome.ttsEngine.onStop.addListener(async () => {
  await EnhancedNetworkTts.clearMojoRequests();
});
