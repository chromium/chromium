// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
import type {ResponsiveControl} from './responsive_control.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * An interface for actions contained within an
 * OverflowableToolbarActionContainer. Actions are hidden if they cannot be
 * entirely fit on the toolbar.
 *
 * Each action must either be a divider or a button. Button actions will
 * appear on the overflow menu if they don't fit.
 */
export interface OverflowableToolbarAction {
  /**
   * Whether or not the element is a divider.
   */
  isDivider(): boolean;

  /**
   * Returns the OverflowMenuItem corresponding to the action to display on the
   * overflow menu when overflowed. Will not be invoked for dividers, which are
   * assumed not to ever appear on the overflow menu, even if hidden due to
   * overflow.
   */
  getOverflowMenuItem(): OverflowMenuItem;
}

// See OverflowableToolbarActionContainerMixin for details.
export interface OverflowableToolbarActionContainer extends ResponsiveControl {
  /**
   * Returns an array containing all current actions within the container. They
   * should be listed from left to right. Actions to the right are given higher
   * priority. When trying to fit actions on the toolbar, if one action won't
   * fit, all lower priority actions within a single
   * OverflowableToolbarActionContainer will also be hidden, though lower
   * priority elements from other ResponsiveControls may still be shown, if they
   * fit.
   *
   * Dividers are treated as a unit with the control to their left - so, e.g.,
   * if the rightmost element is a divider, it's only shown if the next lower
   * priority element (i.e., the one to its left) is shown. And because of the
   * right-to-left priority behavior, if that divider is not shown, no actions
   * at all will be shown.
   *
   * The return value is cached, and updated() is expected to be invoked
   * whenever the return value changes. During updated() calls, the mixin
   * compares the cached value to the new value, and if the number of actions
   * changes, or any action in the old list is not === to the action in the
   * same position in the new list, a new layout of all responsive controls is
   * queued. This working assumes that an element is never converted from a
   * divider to a button, or vice versa, and button elements may have their
   * icons changed, but never have their size changed.
   */
  getActions(): Array<CrLitElement&OverflowableToolbarAction>;
}

/**
 * A mixin for responsive controls that manage multiple child actions
 * and optional dividers. Each child action will be hidden if it doesn't fit.
 * Non-divider actions that don't fit will be added to the overflow menu.
 * Consumers only need to implement getActions(). All non-divider actions must
 * be of equal size.
 *
 * Does not hide the top-level element when all sub-elements are overflowed.
 * That's currently not needed for any subtype, though could be added, if
 * needed.
 */
export const OverflowableToolbarActionContainerMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<OverflowableToolbarActionContainer> => {
      class OverflowableToolbarActionContainer extends superClass implements
          OverflowableToolbarActionContainer {
        // Cached return value of getActions(). Cached primarily so we can
        // detect changes in updated() to trigger layout requests, though it's
        // also used to reduce calls to getActions().
        private cachedActions_: Array<CrLitElement&OverflowableToolbarAction> =
            [];

        // See documentation in OverflowableToolbarActionContainer, above.
        // Subclasses need to override this.
        getActions(): Array<CrLitElement&OverflowableToolbarAction> {
          assertNotReached();
        }

        shouldBeShown(): boolean {
          // Only need to show this if there is at least one action.
          return this.cachedActions_.length > 0;
        }

        setToMinWidth() {
          // Sets all actions as overflowed.
          this.setOverflowed_(this.cachedActions_, true);
        }

        setToPreferredWidth() {
          // Sets all actions as not overflowed.
          this.setOverflowed_(this.cachedActions_, false);
        }

        expandUpToPreferredWidth() {
          // Expects all actions to currently be overflowed due to an earlier
          // setToMinWidth() call. Tries to set overflowed to false for each of
          // them, back-to-front, checking if things fit before going on to the
          // next. If a divider is expanded, it is treated as a unit with the
          // next action (if there is one). If they don't fit, both are hidden
          // again, and we immediately return, leaving all subsequent actions
          // hidden as well.
          const actions = this.cachedActions_;
          const shadowRoot = this.getRootNode() as ShadowRoot;
          const toolbarApp = shadowRoot?.host as ToolbarAppElement;
          assert(toolbarApp);

          for (let i = actions.length - 1; i >= 0; i--) {
            const currentActions = [actions[i]!];

            // If current action is a divider, need to consider it and the
            // next action together.
            if (actions[i]!.isDivider() && i > 0) {
              // Since we are grouping the two actions together, decrement i
              // again for the next loop iteration.
              i--;
              currentActions.push(actions[i]!);
            }

            // Try showing all actions currently under consideration.
            this.setOverflowed_(currentActions, false);

            // If they don't fit, hide them again, and return.
            if (toolbarApp.getAvailableWidth() < 0) {
              this.setOverflowed_(currentActions, true);
              return;
            }
          }
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          // Determine if a new layout is needed, and if so, request a new
          // layout be done.
          const currentActions = this.getActions();
          let layoutNeeded = false;
          if (this.cachedActions_.length !== currentActions.length) {
            layoutNeeded = true;
          } else {
            for (let i = 0; i < currentActions.length; i++) {
              if (this.cachedActions_[i] !== currentActions[i]) {
                layoutNeeded = true;
                break;
              }
            }
          }
          if (layoutNeeded) {
            this.fire('request-layout');
          }

          // Safe to do this after the layout request, as layout happens
          // asynchronously.
          this.cachedActions_ = currentActions;
        }

        controlsToAddToOverflowMenu(): OverflowMenuItem[] {
          const overflowItems: OverflowMenuItem[] = [];
          for (const action of this.cachedActions_) {
            if (!action.isDivider() &&
                action.classList.contains('overflow-display-none')) {
              overflowItems.push(action.getOverflowMenuItem());
            }
          }
          return overflowItems;
        }

        // Helper to hide/show actions due to overflow.
        private setOverflowed_(
            actions: Array<CrLitElement&OverflowableToolbarAction>,
            overflowed: boolean) {
          for (const action of actions) {
            action.classList.toggle('overflow-display-none', overflowed);
          }
        }
      }
      return OverflowableToolbarActionContainer as T &
          Constructor<OverflowableToolbarActionContainer>;
    };
