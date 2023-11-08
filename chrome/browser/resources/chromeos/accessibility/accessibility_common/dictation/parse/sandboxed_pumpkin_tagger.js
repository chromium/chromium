// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as PumpkinConstants from './pumpkin/pumpkin_constants.js';

/**
 * A class that unpacks and loads the pumpkin semantic parser. Runs in a
 * web worker for security purposes.
 */
class SandboxedPumpkinTagger {
  constructor() {
    /** @private {?speech.pumpkin.api.js.PumpkinTagger.PumpkinTagger} */
    this.pumpkinTagger_ = null;
    /** @private {?PumpkinConstants.PumpkinData} */
    this.pumpkinData_ = null;
    this.init_();
  }

  /** @private */
  init_() {
    globalThis.addEventListener(
        'message', (message) => this.onMessage_(message));
    this.sendToBackground_(
        {type: PumpkinConstants.FromPumpkinTaggerCommand.READY});
  }

  /**
   * Called when the background context posts a message to
   * SandboxedPumpkinTagger's web worker.
   * @param {!Event} message
   * @private
   */
  onMessage_(message) {
    const command =
        /** @type {!PumpkinConstants.ToPumpkinTagger} */ (message.data);
    switch (command.type) {
      case PumpkinConstants.ToPumpkinTaggerCommand.LOAD:
        const pumpkinData =
            /** @type {!PumpkinConstants.PumpkinData} */ (command.pumpkinData);
        const locale =
            /** @type {!PumpkinConstants.PumpkinLocale} */ (command.locale);
        this.load_(pumpkinData, locale);
        return;
      case PumpkinConstants.ToPumpkinTaggerCommand.TAG:
        const text = /** @type {string} */ (command.text);
        const numResults = /** @type {number} */ (command.numResults);
        this.tagAndGetNBestHypotheses_(text, numResults);
        return;
      case PumpkinConstants.ToPumpkinTaggerCommand.REFRESH:
        this.refresh_(
            /** @type {!PumpkinConstants.PumpkinLocale} */ (command.locale));
        return;
    }

    throw new Error(`Unrecognized message received in SandboxedPumpkinTagger: ${
        command.type}`);
  }

  /**
   * @param {!PumpkinConstants.FromPumpkinTagger} command
   * @private
   */
  sendToBackground_(command) {
    postMessage(command);
  }

  /**
   * @param {string} text
   * @param {number} numResults
   * @private
   */
  tagAndGetNBestHypotheses_(text, numResults) {
    const results =
        this.pumpkinTagger_.tagAndGetNBestHypotheses(text, numResults);
    this.sendToBackground_(
        {type: PumpkinConstants.FromPumpkinTaggerCommand.TAG_RESULTS, results});
  }

  /**
   * @param {!PumpkinConstants.PumpkinData} data
   * @param {!PumpkinConstants.PumpkinLocale} locale
   * @private
   */
  async load_(data, locale) {
    if (!data) {
      throw new Error(`Can't load pumpkin tagger from empty data`);
    }

    this.pumpkinData_ = data;
    // Unpack the PumpkinTagger JS.
    const pumpkinTaggerBytes = data.js_pumpkin_tagger_bin_js;
    if (!pumpkinTaggerBytes) {
      throw new Error(`Pumpkin tagger bytes must be valid`);
    }
    const pumpkinTaggerFile = new TextDecoder().decode(pumpkinTaggerBytes);
    // Use indirect eval here to ensure the script works in the global scope.
    const indirectEval = eval;
    const pumpkinTaggerModule = indirectEval(pumpkinTaggerFile);
    if (!pumpkinTaggerModule) {
      throw new Error('Failed to eval pumpkin tagger file');
    }
    /**
     * Closure can't recognize pumpkinTaggerModule as a constructor, so suppress
     * the error.
     * @suppress {checkTypes}
     */
    const pumpkinTagger = new pumpkinTaggerModule();

    // The `taggerWasmJsFile` below expects that the corresponding .wasm file
    // lives in the same directory as it. However, since none of these files
    // live in the extension directory, we need to override the fetch method
    // so that it returns the wasm file bytes from `data` when requested.
    globalThis.fetch = async (fileName) => {
      return new Promise(resolve => {
        const response = new Response(null, {
          ok: true,
          status: 200,
        });
        response.arrayBuffer = async () => {
          return new Promise(resolve => {
            resolve(data.tagger_wasm_main_wasm);
          });
        };
        resolve(response);
      });
    };

    const taggerWasmBytes = data.tagger_wasm_main_js;
    if (!taggerWasmBytes) {
      throw new Error(`Pumpkin wasm bytes must be valid`);
    }
    const taggerWasmJsFile = new TextDecoder().decode(taggerWasmBytes);
    // A promise that resolves once the web assembly module loads.
    const wasmLoadPromise = new Promise((resolve) => {
      globalThis['goog']['global']['Module'] = {
        onRuntimeInitialized() {
          resolve();
        },
      };
    });
    // Load the web assembly.
    // Use indirect eval here to ensure the script works in the global scope.
    indirectEval(taggerWasmJsFile);
    await wasmLoadPromise;

    // Initialize from config files.
    const {pumpkinConfig, actionConfig} = this.getConfigsForLocale(locale);
    pumpkinTagger.initializeFromPumpkinConfig(pumpkinConfig);
    pumpkinTagger.loadActionFrame(actionConfig);

    // Save the PumpkinTagger and notify the background context.
    this.pumpkinTagger_ = pumpkinTagger;
    this.sendToBackground_(
        {type: PumpkinConstants.FromPumpkinTaggerCommand.FULLY_INITIALIZED});
  }

  /**
   * Refreshes SandboxedPumpkinTagger in a new locale.
   * @param {!PumpkinConstants.PumpkinLocale} locale
   */
  refresh_(locale) {
    if (!this.pumpkinTagger_) {
      throw new Error(
          'SandboxedPumpkinTagger must be initialized before calling refresh');
    }

    const {pumpkinConfig, actionConfig} = this.getConfigsForLocale(locale);
    this.pumpkinTagger_.initializeFromPumpkinConfig(pumpkinConfig);
    this.pumpkinTagger_.loadActionFrame(actionConfig);
    this.sendToBackground_({
      type: PumpkinConstants.FromPumpkinTaggerCommand.REFRESHED,
    });
  }

  /**
   * @param {!PumpkinConstants.PumpkinLocale} locale
   * @return {!{pumpkinConfig: !ArrayBuffer, actionConfig: !ArrayBuffer}}
   */
  getConfigsForLocale(locale) {
    let pumpkinConfig;
    let actionConfig;
    switch (locale) {
      case PumpkinConstants.PumpkinLocale.EN_US:
        pumpkinConfig = this.pumpkinData_.en_us_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_.en_us_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.FR_FR:
        pumpkinConfig = this.pumpkinData_.fr_fr_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_.fr_fr_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.IT_IT:
        pumpkinConfig = this.pumpkinData_.it_it_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_.it_it_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.DE_DE:
        pumpkinConfig = this.pumpkinData_.de_de_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_.de_de_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.ES_ES:
        pumpkinConfig = this.pumpkinData_.es_es_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_.es_es_action_config_binarypb;
        break;
      default:
        throw new Error(
            `Can't initialize Pumpkin in unsupported locale: ${locale}`);
    }

    if (!pumpkinConfig || !actionConfig) {
      throw new Error(
          `Either pumpkinConfig or actionConfig is invalid for locale: ${
              locale}`);
    }

    return {pumpkinConfig, actionConfig};
  }
}

new SandboxedPumpkinTagger();
