// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality required to show a password to the user.
 */
export const ShowPasswordMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<ShowPasswordMixinInterface> => {
      class ShowPasswordMixin extends superClass {
        static get properties() {
          return {
            isPasswordVisible: {
              type: Boolean,
              value: false,
            },
          };
        }
        isPasswordVisible: boolean;

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

      return ShowPasswordMixin;
    });


export interface ShowPasswordMixinInterface {
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
