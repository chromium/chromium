// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '//resources/js/assert.js';

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {afterNextRender, dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'OobeFocusMixin' is a special mixin which supports focus transferring
 * when a new screen is shown.
 */

type Constructor<T> = new (...args: any[]) => T;

export const OobeFocusMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<OobeFocusMixinInterface> => {
      class OobeFocusMixinInternal extends superClass implements
          OobeFocusMixinInterface {
        /**
         * Called when the screen is shown to handle initial focus.
         */
        focusMarkedElement(root: HTMLElement): void {
          const outerCandidates = root.querySelectorAll('.focus-on-show') || [];
          const nestedCandidates =
              root.shadowRoot?.querySelectorAll('.focus-on-show') || [];
          const focusedElements = [...outerCandidates, ...nestedCandidates];

          let focused = false;
          for (const focusedElement of focusedElements) {
            if (!(focusedElement instanceof HTMLElement) ||
                focusedElement.hidden) {
              continue;
            }

            focused = true;
            afterNextRender(this, () => focusedElement.focus());
            break;
          }
          if (!focused && focusedElements.length > 0) {
            afterNextRender(this, () => {
              const elem = focusedElements[0];
              assertInstanceof(elem, HTMLElement);
              elem.focus();
            });
          }

          this.dispatchEvent(
              new CustomEvent('show-dialog', {bubbles: true, composed: true}));
        }
      }

      return OobeFocusMixinInternal;
    });

export interface OobeFocusMixinInterface {
  focusMarkedElement(root: HTMLElement): void;
}
