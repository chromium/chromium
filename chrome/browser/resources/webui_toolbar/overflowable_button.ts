// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
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
  controlsToAddToOverflowMenu(): OverflowMenuItem[];
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
 *
 * This class is primarily designed to be used in combination with
 * cr-icon-buttons. It expects the CrLitElement to be the ancestor of an
 * element with the id "button". It's that element whose disabled state is
 * checked to determine if the button should be enabled on the overflow menu.
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
          this.classList.add('overflow-display-none');
        }

        // For a button, the preferred width has the button displayed, so
        // removes the "overflow-display-none" class, checks if the parent
        // element fits in the window, and if not, adds back the
        // "overflow-display-none" class. Expects only to be called after an
        // initial setToMinWidth() call, and only if shouldBeShown() returns
        // true.
        expandUpToPreferredWidth() {
          this.setToPreferredWidth();

          const shadowRoot = this.getRootNode() as ShadowRoot;
          const toolbarApp = shadowRoot.host as ToolbarAppElement;
          if (toolbarApp.getAvailableWidth() < 0) {
            this.setToMinWidth();
          }
        }

        // Unconditionally sets width to preferred width without considering
        // window sizing or other control state. Note that this only sets the
        // button not to be hidden due to overflow; it does not affect
        // `shouldBeShown()`.
        setToPreferredWidth() {
          this.classList.remove('overflow-display-none');
        }

        controlsToAddToOverflowMenu(): OverflowMenuItem[] {
          // If this element is hidden or has not overflowed, nothing to add to
          // the overflow menu.
          if (!this.shouldBeShown() ||
              !this.classList.contains('overflow-display-none')) {
            return [];
          }

          // Otherwise, return information about this button. Even disabled
          // buttons should be shown on the menu, if they've overflowed.
          //
          // Check the button element contained within this, if there is one.
          // Fall back to `this` if no such element exists. Shouldn't happen,
          // but makes the TypeScript compiler happy.
          const innerControl =
              this.shadowRoot?.querySelector('#button') || this;
          const id = TrackedElementManager.getElementId(this);
          assert(
              id, `No TrackedElementIdentifier found for element ${this.id}`);
          return [{
            id,
            isEnabled: !innerControl.hasAttribute('disabled'),
          }];
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);
          if (changedProperties.has('state')) {
            const oldState = changedProperties.get('state');
            // If there's potentially been a change in our visibility, request
            // that there be a new layout once all updated() calls have
            // completed.
            if (!oldState ||
                oldState.shouldBeShown !== this.state.shouldBeShown) {
              this.dispatchEvent(new CustomEvent('request-layout', {
                bubbles: true,
                composed: true,
              }));
            }
          }
        }
      }

      return OverflowableButtonMixin as T & Constructor<ResponsiveControl>&
          Constructor<OverflowableButton>;
    };
