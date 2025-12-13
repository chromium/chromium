// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {type MlPredictionLog, OptimizationTarget} from './autofill_ml_internals.mojom-webui.js';
import {getCss} from './log_details.css.js';
import {getHtml} from './log_details.html.js';

export class LogDetailsElement extends CrLitElement {
  static get is() {
    return 'log-details';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      log: {type: Object},
    };
  }

  accessor log: MlPredictionLog = {
    formSignature: '0',
    formUrl: {url: ''},
    modelOutputTypes: [],
    fieldPredictions: [],
    startTime: {internalValue: 0n},
    endTime: {internalValue: 0n},
    duration: {microseconds: 0n},
    optimizationTarget: OptimizationTarget.kUnknown,
  };

  protected getOptimizationTargetText_(target: OptimizationTarget): string {
    switch (target) {
      case OptimizationTarget.kAutofill:
        return 'Autofill';
      case OptimizationTarget.kPassword:
        return 'Password Manager';
      default:
        return 'Unknown';
    }
  }

  private static readonly TOP_PREDICTIONS_COUNT = 5;

  protected getTopPredictions_(probabilities: number[]|null):
      Array<{type: string, probability: number, percentage: string}> {
    if (!probabilities) {
      return [];
    }
    return probabilities
        .map((probability, typeIndex) => {
          const type = this.log.modelOutputTypes[typeIndex];
          return {
            type: type ?? '[ERROR]',
            probability,
            percentage: `${(probability * 100).toFixed(2)}%`,
          };
        })
        .sort((a, b) => b.probability - a.probability)
        .slice(0, LogDetailsElement.TOP_PREDICTIONS_COUNT);
  }

  protected formatFieldTokens_(tokens: string[]): string {
    return tokens.join(' / ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'log-details': LogDetailsElement;
  }
}

customElements.define(LogDetailsElement.is, LogDetailsElement);
