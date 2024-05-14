// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * BaseRowMixin is a mixin that provides the common properties and
 * APIs shared by all settings row components. It is intended to reduce
 * duplicated code among the row components.
 */

import {dedupingMixin, type PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Constructor} from '../../common/types.js';

export interface BaseRowMixinInterface {
  label?: string;
  sublabel?: string;
  icon?: string;
  learnMoreUrl?: string;
  ariaLabel: string|null;
  ariaDescription: string|null;
  getAriaLabel(): string|null;
  getAriaDescription(): string|null;
}

export const BaseRowMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<BaseRowMixinInterface> => {
      class BaseRowMixinImpl extends superClass {
        static get properties() {
          return {
            /**
             * The user-visible label for the row.
             */
            label: String,

            /**
             * The user-visible sublabel for the row.
             */
            sublabel: String,

            /**
             * The leading icon for the row.
             */
            icon: String,

            /**
             * The URL that follows the `sublabel`. It usually opens a help
             * center article to help user better understand the setting.
             */
            learnMoreUrl: {
              type: String,
              reflectToAttribute: true,
            },

            /**
             * Used to manually define an a11y label for the associated control
             * element of the row.
             */
            ariaLabel: {
              type: String,
              reflectToAttribute: true,
            },

            /**
             * Used to manually define an a11y description for the associated
             * control element of the row.
             */
            ariaDescription: {
              type: String,
              reflectToAttribute: true,
            },
          };
        }

        label?: string;
        sublabel?: string;
        icon?: string;
        learnMoreUrl?: string;

        /**
         * Return an a11y label for the associated control element. `ariaLabel`
         * takes precedence over `label`. If neither of them is defined, it
         * returns null.
         */
        getAriaLabel(): string|null {
          if (this.ariaLabel) {
            return this.ariaLabel;
          }

          if (this.label) {
            return this.label;
          }

          return null;
        }

        /**
         * Return an a11y description for the associated control element.
         * `ariaDescription` takes precedence over `sublabel`. If neither of
         * them is defined, it returns null.
         */
        getAriaDescription(): string|null {
          if (this.ariaDescription) {
            return this.ariaDescription;
          }

          if (this.sublabel) {
            return this.sublabel;
          }

          return null;
        }
      }

      return BaseRowMixinImpl;
    });
