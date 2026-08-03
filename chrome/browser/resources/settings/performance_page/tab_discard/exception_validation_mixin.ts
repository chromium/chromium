// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {I18nMixinLitInterface} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {dedupingMixin} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PerformanceBrowserProxy} from '../performance_browser_proxy.js';
import {PerformanceBrowserProxyImpl} from '../performance_browser_proxy.js';

export const MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH = 10 * 1024;

export const TAB_DISCARD_EXCEPTIONS_PREF =
    'performance_tuning.tab_discarding.exceptions_with_time';
export const TAB_DISCARD_EXCEPTIONS_MANAGED_PREF =
    'performance_tuning.tab_discarding.exceptions_managed';

type Constructor<T> = new (...args: any[]) => T;

export const ExceptionValidationMixin = dedupingMixin(
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<ExceptionValidationMixinInterface&I18nMixinLitInterface> => {
      const superClassBase = I18nMixinLit(superClass);
      class ExceptionValidationMixin extends superClassBase implements
          ExceptionValidationMixinInterface {
        static get properties() {
          return {
            errorMessage: {type: String},
            inputInvalid: {type: Boolean},
            rule: {type: String},
            submitDisabled: {type: Boolean, notify: true},
          };
        }

        private browserProxy_: PerformanceBrowserProxy =
            PerformanceBrowserProxyImpl.getInstance();
        accessor errorMessage: string = '';
        accessor inputInvalid: boolean = false;
        accessor rule: string = '';
        accessor submitDisabled: boolean = true;

        validate() {
          const rule = this.rule.trim();

          if (!rule) {
            this.inputInvalid = false;
            this.submitDisabled = true;
            this.errorMessage = '';
            return;
          }

          if (rule.length > MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH) {
            this.inputInvalid = true;
            this.submitDisabled = true;
            this.errorMessage = this.i18n('onStartupUrlTooLong');
            return;
          }

          this.browserProxy_.validateTabDiscardExceptionRule(rule).then(
              valid => {
                this.inputInvalid = !valid;
                this.submitDisabled = !valid;
                this.errorMessage =
                    valid ? '' : this.i18n('onStartupInvalidUrl');
              });
        }
      }

      return ExceptionValidationMixin;
    });

export interface ExceptionValidationMixinInterface {
  errorMessage: string;
  inputInvalid: boolean;
  rule: string;
  submitDisabled: boolean;
  validate(): void;
}
