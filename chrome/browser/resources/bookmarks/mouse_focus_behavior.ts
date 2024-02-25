// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export const HIDE_FOCUS_RING_ATTRIBUTE = 'hide-focus-ring';

type Constructor<T> = new ( ...args: any[]) => T;

/**
 * Behavior which adds the 'hide-focus-ring' attribute to a target element
 * when the user interacts with it using the mouse, allowing the focus outline
 * to be hidden without affecting keyboard users.
 */
export const MouseFocusMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MouseFocusMixinInterface> => {
      class MouseFocusMixin extends superClass {
        private boundOnMousedown_: (e: Event) => void;
        boundOnKeydown: (e: KeyboardEvent) => void;

        override connectedCallback() {
          super.connectedCallback();
          this.boundOnMousedown_ = this.onMousedown_.bind(this);
          this.boundOnKeydown = this.onKeydown_.bind(this);

          // These events are added to the document because capture doesn't work
          // properly when listeners are added to a Polymer element, because the
          // event is considered AT_TARGET for the element, and is evaluated
          // after inner captures.
          document.addEventListener('mousedown', this.boundOnMousedown_, true);
          document.addEventListener('keydown', this.boundOnKeydown, true);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          document.removeEventListener(
              'mousedown', this.boundOnMousedown_, true);
          document.removeEventListener(
              'keydown', this.boundOnKeydown, true);
        }

        private onMousedown_() {
          this.setAttribute(HIDE_FOCUS_RING_ATTRIBUTE, '');
        }

        private onKeydown_(e: KeyboardEvent) {
          if (!['Shift', 'Alt', 'Control', 'Meta'].includes(e.key)) {
            this.removeAttribute(HIDE_FOCUS_RING_ATTRIBUTE);
          }
        }
      }

      return MouseFocusMixin;
    });

export interface MouseFocusMixinInterface {
  boundOnKeydown: (e: KeyboardEvent) => void;
}
