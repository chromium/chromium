// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines constants used for Pumpkin.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

/**
 * The sandbox doesn't have access to extension APIs, so we need to keep a copy
 * of the PumpkinData typedef. Copied from
 * third_party/closure_compiler/externs/accessibility_private.js
 * TODO(crbug.com/1258190): Consider creating a python script that would pull
 * this definition in at build time.
 */
export interface PumpkinData {
  js_pumpkin_tagger_bin_js: ArrayBuffer;
  tagger_wasm_main_js: ArrayBuffer;
  tagger_wasm_main_wasm: ArrayBuffer;
  en_us_action_config_binarypb: ArrayBuffer;
  en_us_pumpkin_config_binarypb: ArrayBuffer;
  fr_fr_action_config_binarypb: ArrayBuffer;
  fr_fr_pumpkin_config_binarypb: ArrayBuffer;
  it_it_action_config_binarypb: ArrayBuffer;
  it_it_pumpkin_config_binarypb: ArrayBuffer;
  de_de_action_config_binarypb: ArrayBuffer;
  de_de_pumpkin_config_binarypb: ArrayBuffer;
  es_es_action_config_binarypb: ArrayBuffer;
  es_es_pumpkin_config_binarypb: ArrayBuffer;
}

/** The types of commands that can come from SandboxedPumpkinTagger. */
export enum FromPumpkinTaggerCommand {
  READY = 'ready',
  FULLY_INITIALIZED = 'fullyInitialized',
  TAG_RESULTS = 'tagResults',
  REFRESHED = 'refreshed',
}

/** The types of commands that can be sent to SandboxedPumpkinTagger. */
export enum ToPumpkinTaggerCommand {
  LOAD = 'load',
  TAG = 'tagAndGetNBestHypotheses',
  REFRESH ='refresh',
}

/** Defines the message data received from SandboxedPumpkinTagger. */
export interface FromPumpkinTagger {
  results?: Object|null;
  type: FromPumpkinTaggerCommand;
}

/** Defines the message data sent to SandboxedPumpkinTagger. */
export interface ToPumpkinTagger {
  locale?: PumpkinLocale;
  numResults?: number;
  pumpkinData?: PumpkinData|null;
  text?: string;
  type: ToPumpkinTaggerCommand;
}

/** Supported Pumpkin locales. */
export enum PumpkinLocale {
  EN_US = 'en_us',
  FR_FR = 'fr_fr',
  IT_IT = 'it_it',
  DE_DE = 'de_de',
  ES_ES = 'es_es',
}

/**
 * Map from BCP-47 locale code (see dictation.cc) to directory name in
 * dictation/parse/pumpkin/ for supported Pumpkin locales.
 * TODO(crbug.com/1264544): Determine if all en* languages can be mapped to
 * en_us. Possible locales are listed in dictation.cc,
 * kWebSpeechSupportedLocales.
 */
export const SUPPORTED_LOCALES = {
  // English.
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
  // French.
  'fr-BE': PumpkinLocale.FR_FR,
  'fr-CA': PumpkinLocale.FR_FR,
  'fr-CH': PumpkinLocale.FR_FR,
  'fr-FR': PumpkinLocale.FR_FR,
  // Italian.
  'it-CH': PumpkinLocale.IT_IT,
  'it-IT': PumpkinLocale.IT_IT,
  // German.
  'de-AT': PumpkinLocale.DE_DE,
  'de-CH': PumpkinLocale.DE_DE,
  'de-DE': PumpkinLocale.DE_DE,
  // Spanish.
  'es-AR': PumpkinLocale.ES_ES,
  'es-BO': PumpkinLocale.ES_ES,
  'es-CL': PumpkinLocale.ES_ES,
  'es-CO': PumpkinLocale.ES_ES,
  'es-CR': PumpkinLocale.ES_ES,
  'es-DO': PumpkinLocale.ES_ES,
  'es-EC': PumpkinLocale.ES_ES,
  'es-ES': PumpkinLocale.ES_ES,
  'es-GT': PumpkinLocale.ES_ES,
  'es-HN': PumpkinLocale.ES_ES,
  'es-MX': PumpkinLocale.ES_ES,
  'es-NI': PumpkinLocale.ES_ES,
  'es-PA': PumpkinLocale.ES_ES,
  'es-PE': PumpkinLocale.ES_ES,
  'es-PR': PumpkinLocale.ES_ES,
  'es-PY': PumpkinLocale.ES_ES,
  'es-SV': PumpkinLocale.ES_ES,
  'es-US': PumpkinLocale.ES_ES,
  'es-UY': PumpkinLocale.ES_ES,
  'es-VE': PumpkinLocale.ES_ES,
};

/**
 * PumpkinTagger Hypothesis argument names. These should match the variable
 * argument placeholders in voiceaccess.patterns_template and the static strings
 * defined in voiceaccess/utils/PumpkinUtils.java in google3.
 */
export enum HypothesisArgumentName {
  SEM_TAG = 'SEM_TAG',
  NUM_ARG = 'NUM_ARG',
  OPEN_ENDED_TEXT = 'OPEN_ENDED_TEXT',
  BEGIN_PHRASE = 'BEGIN_PHRASE',
  END_PHRASE = 'END_PHRASE',
}

export const SANDBOXED_PUMPKIN_TAGGER_JS_FILE =
    'dictation/parse/sandboxed_pumpkin_tagger.js';

TestImportManager.exportForTesting(['SUPPORTED_LOCALES', SUPPORTED_LOCALES]);
