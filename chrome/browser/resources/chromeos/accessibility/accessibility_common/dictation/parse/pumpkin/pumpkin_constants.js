// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines constants used for Pumpkin.
 */

/**
 * The sandbox doesn't have access to extension APIs, so we need to keep a copy
 * of the PumpkinData typedef. Copied from
 * third_party/closure_compiler/externs/accessibility_private.js
 * TODO(crbug.com/1258190): Consider creating a python script that would pull
 * this definition in at build time.
 * @typedef {{
 *   js_pumpkin_tagger_bin_js: ArrayBuffer,
 *   tagger_wasm_main_js: ArrayBuffer,
 *   tagger_wasm_main_wasm: ArrayBuffer,
 *   en_us_action_config_binarypb: ArrayBuffer,
 *   en_us_pumpkin_config_binarypb: ArrayBuffer,
 *   fr_fr_action_config_binarypb: ArrayBuffer,
 *   fr_fr_pumpkin_config_binarypb: ArrayBuffer,
 *   it_it_action_config_binarypb: ArrayBuffer,
 *   it_it_pumpkin_config_binarypb: ArrayBuffer,
 *   de_de_action_config_binarypb: ArrayBuffer,
 *   de_de_pumpkin_config_binarypb: ArrayBuffer,
 *   es_es_action_config_binarypb: ArrayBuffer,
 *   es_es_pumpkin_config_binarypb: ArrayBuffer
 * }}
 */
export let PumpkinData;

/**
 * The types of commands that can come from SandboxedPumpkinTagger.
 * @enum {string}
 */
export const FromPumpkinTaggerCommand = {
  READY: 'ready',
  FULLY_INITIALIZED: 'fullyInitialized',
  TAG_RESULTS: 'tagResults',
};

/**
 * The types of commands that can be sent to SandboxedPumpkinTagger.
 * @enum {string}
 */
export const ToPumpkinTaggerCommand = {
  LOAD: 'load',
  TAG: 'tagAndGetNBestHypotheses',
};

/**
 * Defines the message data received from SandboxedPumpkinTagger.
 * @typedef {{
 *  results: (!Object|null|undefined),
 *  type: !FromPumpkinTaggerCommand,
 * }}
 */
export let FromPumpkinTagger;

/**
 * Defines the message data sent to SandboxedPumpkinTagger.
 * @typedef {{
 *  locale: (!PumpkinLocale|undefined),
 *  numResults: (number|undefined),
 *  pumpkinData: (!PumpkinData|null|undefined),
 *  text: (string|undefined),
 *  type: !ToPumpkinTaggerCommand,
 * }}
 */
export let ToPumpkinTagger;

/**
 * Supported Pumpkin locales.
 * @enum {string}
 */
export const PumpkinLocale = {
  EN_US: 'en_us',
  FR_FR: 'fr_fr',
  IT_IT: 'it_it',
  DE_DE: 'de_de',
  ES_ES: 'es_es',
};

/**
 * Map from BCP-47 locale code (see dictation.cc) to directory name in
 * dictation/parse/pumpkin/ for supported Pumpkin locales.
 * TODO(crbug.com/1264544): Determine if all en* languages can be mapped to
 * en_us. Possible locales are listed in dictation.cc,
 * kWebSpeechSupportedLocales.
 * TODO(https://crbug.com/1258190): Add mappings for other locales supported by
 * Pumpkin.
 * @const {!Object<string, PumpkinLocale>}
 */
export const SUPPORTED_LOCALES = {
  'en-US': PumpkinLocale.EN_US,
  'en-AU': PumpkinLocale.EN_US,
  'en-CA': PumpkinLocale.EN_US,
  'en-GB': PumpkinLocale.EN_US,
  'en-GH': PumpkinLocale.EN_US,
  'en-HK': PumpkinLocale.EN_US,
  'en-IN': PumpkinLocale.EN_US,
  'en-KE': PumpkinLocale.EN_US,
  'en-NG': PumpkinLocale.EN_US,
  'en-NZ': PumpkinLocale.EN_US,
  'en-PH': PumpkinLocale.EN_US,
  'en-PK': PumpkinLocale.EN_US,
  'en-SG': PumpkinLocale.EN_US,
  'en-TZ': PumpkinLocale.EN_US,
  'en-ZA': PumpkinLocale.EN_US,
};

/**
 * PumpkinTagger Hypothesis argument names. These should match the variable
 * argument placeholders in voiceaccess.patterns_template and the static strings
 * defined in voiceaccess/utils/PumpkinUtils.java in google3.
 * @enum {string}
 */
export const HypothesisArgumentName = {
  SEM_TAG: 'SEM_TAG',
  NUM_ARG: 'NUM_ARG',
  OPEN_ENDED_TEXT: 'OPEN_ENDED_TEXT',
};

/** @const {string} */
export const SANDBOXED_PUMPKIN_TAGGER_JS_FILE =
    'dictation/parse/sandboxed_pumpkin_tagger.js';
