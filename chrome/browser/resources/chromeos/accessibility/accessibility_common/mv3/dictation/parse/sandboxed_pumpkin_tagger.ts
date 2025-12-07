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
    this.init_();
  }

  private pumpkinTagger_: speech.pumpkin.api.js.PumpkinTagger|null = null;
  private pumpkinData_: PumpkinConstants.PumpkinData|null = null;

  private init_(): void {
    globalThis.addEventListener(
        'message', (message) => this.onMessage_(message));
    this.sendToBackground_(
        {type: PumpkinConstants.FromPumpkinTaggerCommand.READY});
  }

  /**
   * Called when the background context posts a message to
   * SandboxedPumpkinTagger's web worker.
   */
  private onMessage_(message: MessageEvent): void {
    const command: PumpkinConstants.ToPumpkinTagger = message.data;
    switch (command.type) {
      case PumpkinConstants.ToPumpkinTaggerCommand.LOAD:
        if (!command.pumpkinData) {
          throw new Error(`Can't load pumpkin tagger from empty data`);
        }
        if (!command.locale) {
          throw new Error(`Can't load pumpkin tagger from empty locale`);
        }
        this.load_(command.pumpkinData, command.locale);
        return;
      case PumpkinConstants.ToPumpkinTaggerCommand.TAG:
        // TODO(crbug.com/314203187): Not null asserted, check that this is
        // correct.
        this.tagAndGetNBestHypotheses_(command.text!, command.numResults!);
        return;
      case PumpkinConstants.ToPumpkinTaggerCommand.REFRESH:
        if (!command.locale) {
          throw new Error(`Can't load pumpkin tagger from empty locale`);
        }
        this.refresh_(command.locale);
        return;
      default:
        throw new Error(
            `Unrecognized message received in SandboxedPumpkinTagger: ${
                command.type}`);
    }
  }

  private sendToBackground_(command: PumpkinConstants.FromPumpkinTagger): void {
    window.parent.postMessage(command, '*');
  }

  private tagAndGetNBestHypotheses_(text: string, numResults: number): void {
    // TODO(crbug.com/314203187): Not null asserted, check that this is correct.
    const results =
        this.pumpkinTagger_!.tagAndGetNBestHypotheses(text, numResults);
    this.sendToBackground_(
        {type: PumpkinConstants.FromPumpkinTaggerCommand.TAG_RESULTS, results});
  }

  private async load_(
      data: PumpkinConstants.PumpkinData,
      locale: PumpkinConstants.PumpkinLocale) {
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
    globalThis.fetch = async () => {
      return new Promise(resolve => {
        const response = new Response(null, {
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
    const wasmLoadPromise = new Promise<void>(resolve => {
      (globalThis as any)['goog']['global']['Module'] = {
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
   */
  refresh_(locale: PumpkinConstants.PumpkinLocale): void {
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

  getConfigsForLocale(locale: PumpkinConstants.PumpkinLocale):
      {pumpkinConfig: ArrayBuffer; actionConfig: ArrayBuffer} {
    let pumpkinConfig;
    let actionConfig;
    // TODO(crbug.com/314203187): Not null asserted, check that this is
    // correct.
    switch (locale) {
      case PumpkinConstants.PumpkinLocale.EN_US:
        pumpkinConfig = this.pumpkinData_!.en_us_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_!.en_us_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.FR_FR:
        pumpkinConfig = this.pumpkinData_!.fr_fr_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_!.fr_fr_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.IT_IT:
        pumpkinConfig = this.pumpkinData_!.it_it_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_!.it_it_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.DE_DE:
        pumpkinConfig = this.pumpkinData_!.de_de_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_!.de_de_action_config_binarypb;
        break;
      case PumpkinConstants.PumpkinLocale.ES_ES:
        pumpkinConfig = this.pumpkinData_!.es_es_pumpkin_config_binarypb;
        actionConfig = this.pumpkinData_!.es_es_action_config_binarypb;
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

declare global {
  var pumpkinTagger: SandboxedPumpkinTagger;
}
globalThis.pumpkinTagger = new SandboxedPumpkinTagger();
