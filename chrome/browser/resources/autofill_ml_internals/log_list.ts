// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {type Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {type MlPredictionLog, OptimizationTarget} from './autofill_ml_internals.mojom-webui.js';
import {getCss} from './log_list.css.js';
import {getHtml} from './log_list.html.js';


export class LogListElement extends CrLitElement {
  static get is() {
    return 'log-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      logEntries: {type: Array},
      selectedLogEntry: {type: Object},
      hideAutofill_: {type: Boolean},
      hidePasswordManager_: {type: Boolean},
    };
  }

  accessor logEntries: MlPredictionLog[] = [];
  accessor selectedLogEntry: MlPredictionLog|undefined;
  protected accessor hideAutofill_: boolean = false;
  protected accessor hidePasswordManager_: boolean = false;

  protected get filteredLogEntries_(): MlPredictionLog[] {
    return this.logEntries.filter(log => {
      switch (log.optimizationTarget) {
        case OptimizationTarget.kAutofill:
          return !this.hideAutofill_;
        case OptimizationTarget.kPassword:
          return !this.hidePasswordManager_;
        case OptimizationTarget.kUnknown:
          return true;
        default:
          const exhaustiveCheck: never = log.optimizationTarget;
          throw new Error(`Unhandled Enum case: ${exhaustiveCheck}`);
      }
    });
  }

  protected onLogClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const log = this.filteredLogEntries_[index];
    this.selectedLogEntry = log;
    this.fire('log-selected', log);
  }

  protected onAutofillCheckboxChange_(e: Event) {
    const target = e.target as HTMLInputElement;
    this.updateFilter_(OptimizationTarget.kAutofill, target.checked);
  }

  protected onPasswordManagerCheckboxChange_(e: Event) {
    const target = e.target as HTMLInputElement;
    this.updateFilter_(OptimizationTarget.kPassword, target.checked);
  }

  private updateFilter_(target: OptimizationTarget, enabled: boolean) {
    switch (target) {
      case OptimizationTarget.kAutofill:
        this.hideAutofill_ = !enabled;
        break;
      case OptimizationTarget.kPassword:
        this.hidePasswordManager_ = !enabled;
        break;
      case OptimizationTarget.kUnknown:
        break;
      default:
        const exhaustiveCheck: never = target;
        throw new Error(`Unhandled Enum case: ${exhaustiveCheck}`);
    }
  }

  protected onDownloadClick_() {
    const data = JSON.stringify(this.logEntries, (_, value) => {
      return typeof value === 'bigint' ? value.toString() : value;
    }, 2);
    const blob = new Blob([data], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'autofill_ml_logs.json';
    a.click();
    URL.revokeObjectURL(url);
  }

  protected getChipClass_(target: OptimizationTarget): string {
    switch (target) {
      case OptimizationTarget.kAutofill:
        return 'autofill';
      case OptimizationTarget.kPassword:
        return 'password';
      case OptimizationTarget.kUnknown:
        return '';
      default:
        const exhaustiveCheck: never = target;
        throw new Error(`Unhandled Enum case: ${exhaustiveCheck}`);
    }
  }

  protected getOptimizationTargetText_(target: OptimizationTarget): string {
    switch (target) {
      case OptimizationTarget.kAutofill:
        return 'Autofill';
      case OptimizationTarget.kPassword:
        return 'PWM';
      case OptimizationTarget.kUnknown:
        return 'Unknown';
      default:
        const exhaustiveCheck: never = target;
        throw new Error(`Unhandled Enum case: ${exhaustiveCheck}`);
    }
  }

  protected formatTime_(time: Time): string {
    const date = new Date(Number(time.internalValue / 1000n));
    return date.toLocaleTimeString();
  }

  protected getPluralizedFields_(count: number): string {
    return `${count} field${count === 1 ? '' : 's'}`;
  }

  protected getSelectedCssClass_(item: MlPredictionLog): string {
    return this.selectedLogEntry === item ? 'selected' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'log-list': LogListElement;
  }
}

customElements.define(LogListElement.is, LogListElement);
