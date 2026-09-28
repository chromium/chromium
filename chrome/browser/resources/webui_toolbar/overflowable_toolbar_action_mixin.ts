// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

type Constructor<T> = new (...args: any[]) => T;

export interface OverflowableToolbarActionMixinInterface {
  /**
   * Whether the action should never be hidden due to overflow. Ignored for
   * dividers, which are shown or hidden based on the action immediately to
   * their left.
   */
  preventOverflow: boolean;

  /**
   * Whether or not the element is a divider. This is not a reactive property,
   * and is expected to never change after construction. Divider subclasses
   * should override it with a class field initialized to true.
   */
  isDivider: boolean;

  /**
   * Returns the OverflowMenuItem corresponding to the action to display on the
   * overflow menu when overflowed. Will not be invoked for dividers, which are
   * assumed not to ever appear on the overflow menu, even if hidden due to
   * overflow.
   */
  getOverflowMenuItem(): OverflowMenuItem;
}

// Elements within an OverflowableToolbarActionContainer. They must all use
// OverflowableToolbarActionMixin.
export type OverflowableToolbarActionElement =
    CrLitElement&OverflowableToolbarActionMixinInterface;

/**
 * A mixin for actions contained within an OverflowableToolbarActionContainer.
 * Actions are hidden if they cannot be entirely fit on the toolbar.
 *
 * Each action must either be a divider or a button. Button actions will
 * appear on the overflow menu if they don't fit, and must override
 * getOverflowMenuItem().
 *
 * Requests a new layout whenever `preventOverflow` changes, as that can
 * affect which controls should be overflowed.
 */
export const OverflowableToolbarActionMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<OverflowableToolbarActionMixinInterface> => {
      class OverflowableToolbarActionMixin extends superClass implements
          OverflowableToolbarActionMixinInterface {
        static get properties() {
          return {
            preventOverflow: {type: Boolean},
          };
        }

        accessor preventOverflow: boolean = false;

        // This is not expected to change after construction, so is not a
        // reactive property.
        isDivider: boolean = false;

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          // Only request a layout if `preventOverflow` was modified after it
          // was initialized, which is indicated by the old value being
          // defined. Initial values are included in `changedProperties` during
          // the first update, but containers already request a layout when an
          // action is added to or removed from them.
          //
          // When `preventOverflow` goes from false to true, and the element is
          // visible, a new layout is not needed, but this requests one anyway.
          // This currently is not a common case, so doesn't seem worth the
          // effort of handling.
          if (changedProperties.get('preventOverflow') !== undefined) {
            this.fire('request-layout');
          }
        }

        // See documentation in OverflowableToolbarActionMixinInterface, above.
        // Must be overridden by non-divider actions.
        getOverflowMenuItem(): OverflowMenuItem {
          assertNotReached(
              'getOverflowMenuItem() must be overridden by non-divider ' +
              'actions, and never called on dividers');
        }
      }

      return OverflowableToolbarActionMixin;
    };
