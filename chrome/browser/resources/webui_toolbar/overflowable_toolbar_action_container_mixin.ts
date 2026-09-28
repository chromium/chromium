// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
import type {OverflowableToolbarActionElement} from './overflowable_toolbar_action_mixin.js';
import type {ResponsiveControl} from './responsive_control.js';

type Constructor<T> = new (...args: any[]) => T;

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
   * queued. This assumes that an element is never converted from a
   * divider to a button, or vice versa, and button elements may have their
   * icons changed, but never have their size changed.
   */
  getActions(): OverflowableToolbarActionElement[];
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
        private cachedActions_: OverflowableToolbarActionElement[] = [];

        // See documentation in OverflowableToolbarActionContainer, above.
        // Subclasses need to override this.
        getActions(): OverflowableToolbarActionElement[] {
          assertNotReached();
        }

        shouldBeShown(): boolean {
          // Only need to show this if there is at least one action.
          return this.cachedActions_.length > 0;
        }

        setToMinWidth() {
          // Sets actions as overflowed, except for actions where
          // `preventOverflow` is true (and any divider associated with a
          // prevent-overflow action).
          for (const group of this.getActionGroups_()) {
            const action = group[0]!;
            this.setOverflowed_(group, !action.preventOverflow);
          }
        }

        setToPreferredWidth() {
          // Sets all actions as not overflowed.
          this.setOverflowed_(this.cachedActions_, false);
        }

        expandUpToPreferredWidth() {
          // Expects actions to currently be overflowed due to an earlier
          // setToMinWidth() call. Tries to set overflowed to false for each
          // action group, from highest priority to lowest. If an action group
          // is expanded and doesn't fit, it is hidden again, and we immediately
          // return, leaving all lower priority action groups hidden as well.
          const shadowRoot = this.getRootNode() as ShadowRoot;
          const toolbarApp = shadowRoot?.host as ToolbarAppElement;
          assert(toolbarApp);

          for (const group of this.getActionGroups_()) {
            // Skip any prevent-overflow groups, since they should already be
            // visible.
            if (group[0]!.preventOverflow) {
              continue;
            }

            // Try showing all actions in the current group.
            this.setOverflowed_(group, false);

            // If they don't fit, hide them again, and return.
            if (toolbarApp.getAvailableWidth() < 0) {
              this.setOverflowed_(group, true);
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
            if (!action.isDivider &&
                action.classList.contains('overflow-display-none')) {
              overflowItems.push(action.getOverflowMenuItem());
            }
          }
          return overflowItems;
        }

        // Groups actions and dividers from highest priority (rightmost) to
        // lowest priority (leftmost). Dividers are grouped with the action to
        // their left: [action, divider], so that consumers can examine group[0]
        // to check the non-divider element.
        private getActionGroups_(): OverflowableToolbarActionElement[][] {
          const groups: OverflowableToolbarActionElement[][] = [];
          const actions = this.cachedActions_;
          for (let i = actions.length - 1; i >= 0; i--) {
            if (actions[i]!.isDivider) {
              // The leftmost element being a divider is currently not
              // supported. If we ever do add that, we'll need to figure out
              // visibility rules for that case.
              assert(i > 0);
              groups.push([actions[i - 1]!, actions[i]!]);
              i--;
            } else {
              groups.push([actions[i]!]);
            }
          }
          return groups;
        }

        // Helper to hide/show actions due to overflow.
        private setOverflowed_(
            actions: OverflowableToolbarActionElement[], overflowed: boolean) {
          for (const action of actions) {
            action.classList.toggle('overflow-display-none', overflowed);
          }
        }
      }
      return OverflowableToolbarActionContainer as T &
          Constructor<OverflowableToolbarActionContainer>;
    };
