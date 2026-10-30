// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {dedupingMixin} from 'chrome://resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality required to show a password to the user.
 */
export const ShowPasswordMixinLit = dedupingMixin(
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<ShowPasswordMixinLitInterface> => {
      class ShowPasswordMixinLit extends superClass implements
          ShowPasswordMixinLitInterface {
        static get properties() {
          return {
            isPasswordVisible: {type: Boolean},
          };
        }
        accessor isPasswordVisible: boolean = false;

        getPasswordInputType() {
          return this.isPasswordVisible ? 'text' : 'password';
        }

        getShowHideButtonLabel() {
          return this.isPasswordVisible ?
              loadTimeData.getString('hidePassword') :
              loadTimeData.getString('showPassword');
        }

        getShowHideButtonIconClass() {
          return this.isPasswordVisible ? 'icon-visibility-off' :
                                          'icon-visibility';
        }

        /**
         * Handler for showing/hiding the passwords. This method should be
         * attached to on-click event of show/hide password button.
         */
        onShowHidePasswordButtonClick() {
          this.isPasswordVisible = !this.isPasswordVisible;
        }
      }

      return ShowPasswordMixinLit;
    });


export interface ShowPasswordMixinLitInterface {
  isPasswordVisible: boolean;

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * or when there's federated text otherwise 'password'.
   */
  getPasswordInputType(): string;

  /**
   * Gets the a11y label for the show/hide button.
   */
  getShowHideButtonLabel(): string;

  /**
   * Get the right icon to display when hiding/showing a password.
   */
  getShowHideButtonIconClass(): string;

  /** Handler for clicking the show/hide button. */
  onShowHidePasswordButtonClick(): void;
}
