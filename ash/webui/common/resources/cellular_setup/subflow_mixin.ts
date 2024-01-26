// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer mixin for dealing with Cellular setup subflows.
 * It includes some methods and property shared between subflows.
 */
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonBarState} from './cellular_types.js';

type Constructor<T> = new (...args: any[]) => T;

export const SubflowMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<SubflowMixinInterface> => {
      class SubflowMixin extends superClass implements SubflowMixinInterface {
        static get properties() {
          return {
            buttonState: {
              type: Object,
              notify: true,
            },
          };
        }

        buttonState: ButtonBarState;

        initSubflow(): void {
          assertNotReached();
        }

        navigateForward(): void {
          assertNotReached();
        }

        navigateBackward(): void {
          assertNotReached();
        }
      }

      return SubflowMixin;
    });

export interface SubflowMixinInterface {
  buttonState: ButtonBarState;
  initSubflow(): void;
  navigateForward(): void;
  navigateBackward(): void;
}
