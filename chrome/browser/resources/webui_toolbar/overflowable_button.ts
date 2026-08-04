// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {ResponsiveControl} from './responsive_control.js';

export interface OverflowableButtonState {
  shouldBeShown: boolean;
}

type Constructor<T> = new (...args: any[]) => T;

export interface OverflowableButton {
  state: OverflowableButtonState;
  shouldBeShown(): boolean;
  setToMinWidth(): void;
  expandUpToPreferredWidth(): void;
  setToPreferredWidth(): void;
}

/**
 * A mixin for buttons and controls that should be set to "display: none" and
 * added to the overflow menu if they don't fit on the toolbar.
 *
 * This mixin requires the subclassing button's properties to have a state
 * field which has a `shouldBeShown` value that is true when the button is
 * pinned, and false when it is not and thus should always be hidden. The
 * value is used as an optimization to indicate whether the button should
 * take part in layout at all. It does not handle hiding the button if
 * `shouldBeShown` is false - that's currently expected to be done by the
 * child class.
 */
export const OverflowableButtonMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<ResponsiveControl>&Constructor<OverflowableButton> => {
      class OverflowableButtonMixin extends superClass implements
          ResponsiveControl {
        static get properties() {
          return {
            state: {type: Object},
          };
        }

        accessor state: OverflowableButtonState = {
          // True if the button is pinned / should be shown if there's space for
          // it. See class docs for more details.
          shouldBeShown: false,
        };

        shouldBeShown(): boolean {
          return this.state.shouldBeShown;
        }

        // The minimum width hides the button using `overflow-display-none`.
        setToMinWidth() {
          this.toggleAttribute('overflow-display-none', true);
        }

        // For a button, the preferred width has the button displayed, so
        // removes the "overflow-display-none" property, checks if the parent
        // element fits in the window, and if not, adds back the
        // "overflow-display-none" property. Expects only to be called after an
        // initial setToMinWidth() call, and only if shouldBeShown() returns
        // true.
        expandUpToPreferredWidth() {
          this.setToPreferredWidth();

          const shadowRoot = this.getRootNode() as ShadowRoot;
          const parent = shadowRoot.host as HTMLElement;
          if (parent && parent.clientWidth > window.innerWidth) {
            this.setToMinWidth();
          }
        }

        // Unconditionally sets width to preferred width without considering
        // window sizing or other control state. Note that this only sets the
        // button not to be hidden due to overflow; it does not affect
        // `shouldBeShown()`.
        setToPreferredWidth() {
          this.toggleAttribute('overflow-display-none', false);
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);
          if (changedProperties.has('state')) {
            // When hiding a control, we set it to its preferred width so that
            // when it's shown again, it affects the size of the toolbar,
            // triggering a layout. We could do this on hide rather than show,
            // but only having `overflow-display-none` on controls in the
            // overflow menu is a better invariant.
            if (!this.shouldBeShown()) {
              this.setToPreferredWidth();
            }
          }
        }
      }

      return OverflowableButtonMixin as T & Constructor<ResponsiveControl>&
          Constructor<OverflowableButton>;
    };
