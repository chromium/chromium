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

// A group of actions, and the divider to their right, if any. If any action in
// a group is visible (not hidden due to overflow), the divider must also be
// visible.
interface ActionGroup {
  // The non-divider actions in the group. Never empty.
  actions: OverflowableToolbarActionElement[];
  // The divider to the right of `actions`, if there is one.
  divider: OverflowableToolbarActionElement|null;
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
   * Dividers never contribute menu items to the overflow menu, as it has its
   * own logic to determine divider placement. Dividers split actions into
   * groups, with each divider belonging to the group of actions to its left.
   * A divider is shown whenever any action in its group is shown, and hidden
   * otherwise. The leftmost element may not be a divider, and dividers may not
   * be adjacent to each other, so every divider has at least one action in its
   * group.
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
          // `preventOverflow` is true. Each divider is shown only if an action
          // in its group is shown.
          for (const group of this.getActionGroupsInPriorityOrder_()) {
            let groupHasVisibleAction = false;
            for (const action of group.actions) {
              this.setOverflowed_([action], !action.preventOverflow);
              groupHasVisibleAction ||= action.preventOverflow;
            }
            if (group.divider) {
              this.setOverflowed_([group.divider], !groupHasVisibleAction);
            }
          }
        }

        setToPreferredWidth() {
          // Sets all actions as not overflowed.
          this.setOverflowed_(this.cachedActions_, false);
        }

        expandUpToPreferredWidth() {
          // Expects actions to currently be overflowed due to an earlier
          // setToMinWidth() call. Tries to set overflowed to false for each
          // hidden action, from highest priority to lowest. A hidden divider is
          // shown together with the first action shown from its group. If we
          // try to show an action (or action+divider pair) and it doesn't fit,
          // it is hidden again, and we immediately return, leaving all lower
          // priority actions hidden.
          const shadowRoot = this.getRootNode() as ShadowRoot;
          const toolbarApp = shadowRoot?.host as ToolbarAppElement;
          assert(toolbarApp);

          for (const group of this.getActionGroupsInPriorityOrder_()) {
            // If the divider is hidden, setToMinWidth() hid all actions in the
            // group, so the divider needs to be made visible if the first
            // action in the group becomes visible.
            let hiddenDivider: OverflowableToolbarActionElement|null = null;
            if (group.divider && this.isOverflowed_(group.divider)) {
              hiddenDivider = group.divider;
            }
            for (const action of group.actions) {
              // Skip anything that's already visible - setToMinWidth() should
              // have been called before, leaving all `preventOverflow` actions
              // visible.
              if (!this.isOverflowed_(action)) {
                assert(!hiddenDivider);
                continue;
              }

              const toShow = hiddenDivider ? [action, hiddenDivider] : [action];
              hiddenDivider = null;

              // Try showing the action, and the divider, if needed.
              this.setOverflowed_(toShow, false);

              // If they don't fit, hide them again, and return.
              if (toolbarApp.getAvailableWidth() < 0) {
                this.setOverflowed_(toShow, true);
                return;
              }
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
            if (!action.isDivider && this.isOverflowed_(action)) {
              overflowItems.push(action.getOverflowMenuItem());
            }
          }
          return overflowItems;
        }

        private isOverflowed_(action: OverflowableToolbarActionElement):
            boolean {
          return action.classList.contains('overflow-display-none');
        }

        // Splits `cachedActions_` into groups, each ending with a divider,
        // except possibly the rightmost group. Returns the groups in priority
        // order, from right to left, with the actions within each group also
        // ordered from right to left.
        private getActionGroupsInPriorityOrder_(): ActionGroup[] {
          const groups: ActionGroup[] = [];
          let actions: OverflowableToolbarActionElement[] = [];
          for (const action of this.cachedActions_) {
            if (action.isDivider) {
              // This follows from the restrictions documented with
              // getActions(): No two dividers may be adjacent, and the first
              // element may not be a divider.
              assert(actions.length > 0);
              groups.push({actions: actions.reverse(), divider: action});
              actions = [];
            } else {
              actions.push(action);
            }
          }
          if (actions.length > 0) {
            groups.push({actions: actions.reverse(), divider: null});
          }
          return groups.reverse();
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
