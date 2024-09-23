// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Oobe} from '../../cr_ui.js';
import {OobeUiState} from '../display_manager_types.js';
import {OobeTypes} from '../oobe_types.js';

import {OobeBaseMixin, OobeBaseMixinInterface} from './oobe_base_mixin.js';

/**
 * @fileoverview
 * 'LoginScreenMixin' is login.Screen API implementation for Polymer objects.
 */

type Constructor<T> = new (...args: any[]) => T;

export const LoginScreenMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<LoginScreenMixinInterface> => {
      const superClassBase = OobeBaseMixin(superClass);
      class LoginScreenMixinInternal extends superClassBase implements
          LoginScreenMixinInterface {
        // List of methods exported to login.screenName.<method> API.
        // This is expected to be overridden by the Polymer object using this
        // mixin.
        get EXTERNAL_API(): string[] {
          return [];
        }
        private sendPrefix: string|undefined;

        /**
         * Initialize screen mixin.
         * @param screenName Name of created class (external api prefix).
         */
        initializeLoginScreen(screenName: string): void {
          const api: Record<string, Function> = {};
          for (const methodName of this.EXTERNAL_API) {
            assert(
                (methodName in this) &&
                    typeof this[methodName as keyof typeof this] === 'function',
                'External method "' + methodName + '" for screen "' +
                    screenName + '" is not a function or is undefined.');
            api[methodName] = (...args: any[]) =>
                (this[methodName as keyof typeof this] as Function)(...args);
          }

          this.sendPrefix = 'login.' + screenName + '.userActed';
          this.registerScreenApi(screenName, api);
          Oobe.getInstance().registerScreen(this);
        }

        userActed(args: string|any[]): void {
          if (this.sendPrefix === undefined) {
            console.error('LoginScreenMixin: sendPrefix is undefined');
            return;
          }
          if (typeof args === 'string') {
            args = [args];
          }
          chrome.send(this.sendPrefix, args);
        }

        /* ******************  Default screen API below.  ******************* */

        // If defined, invoked when CANCEL accelerator is pressed.
        cancel: () => void | undefined;

        /**
         * Returns UI state to be used when showing this screen. Default
         * implementation returns OobeUiState.HIDDEN.
         * @return The state of the OOBE UI.
         */
        // eslint-disable-next-line @typescript-eslint/naming-convention
        getOobeUIInitialState(): OobeUiState {
          return OobeUiState.HIDDEN;
        }

        /**
         * Invoked for the currently active screen when screen localized data
         * needs to be updated.
         */
        updateLocalizedContent(): void {}

        /**
         * If defined, invoked when OOBE configuration is loaded.
         */
        updateOobeConfiguration(_configuration: OobeTypes.OobeConfiguration):
            void {}

        /**
         * Register external screen API with login object.
         * Example:
         *    this.registerScreenApi('ScreenName', {
         *         foo() { console.log('foo'); },
         *     });
         *     login.ScreenName.foo(); // valid
         *
         * @param name Name of created class.
         * @param api Screen API.
         */
        private registerScreenApi(name: string, api: Record<string, Function>) {
          // TODO(b/260015147) - Improve this.
          if (globalThis.login === undefined) {
            globalThis.login = {};
          }
          assert('login' in globalThis);
          globalThis.login[name] = api;
        }
      }

      return LoginScreenMixinInternal;
    });

export interface LoginScreenMixinInterface extends OobeBaseMixinInterface {
  initializeLoginScreen(screenName: string): void;
  userActed(action_id: string|any[]): void;
  getOobeUIInitialState(): OobeUiState;
  get EXTERNAL_API(): string[];
  updateLocalizedContent(): void;
  updateOobeConfiguration(configuration: OobeTypes.OobeConfiguration): void;
}

declare global {
  // TODO(b/260015147) - Improve this.
  /* eslint-disable-next-line no-var */
  var login: Record<string, Record<string, Function>>;
}
