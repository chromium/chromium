// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';
import type {ContextMenuType} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {ResponsiveControl} from './responsive_control.js';
import {getContextMenuPosition} from './toolbar_button.js';

export interface OverflowableButtonState {
  shouldBeShown: boolean;
  isContextMenuVisible?: boolean;
  menuOpenToken?: number;
}

type Constructor<T> = new (...args: any[]) => T;

export interface OverflowableButton {
  state: OverflowableButtonState;
  shouldBeShown(): boolean;
  setToMinWidth(): void;
  expandUpToPreferredWidth(): void;
  setToPreferredWidth(): void;
  controlsToAddToOverflowMenu(): OverflowMenuItem[];
  showContextMenuAndPreventOverflow(
      menuType: ContextMenuType, sourceType: MenuSourceType): void;
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
 * It also has support for buttons with context menus. When
 * `state.isContextMenuVisible` is true, the button will not be hidden due to
 * overflow (though if `shouldBeShown` is false, the button is still assumed to
 * be hidden by the child button class's logic). If a control has
 * a `state.menuOpenToken` field, invoking
 * `this.showContextMenuAndPreventOverflow` method will cause the class to keep
 * `state.isContextMenuVisible` set to true until the browser process has echoed
 * back the `menuOpenToken` value. This mixin is expected to be the sole class
 * to manage `menuOpenToken` for a button that uses it.
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

        // Local count of context menu requests sent to the browser. Wraps
        // around at 2^32.
        private menuOpenToken_: number = 0;

        shouldBeShown(): boolean {
          return this.state.shouldBeShown;
        }

        // The minimum width hides the button using `overflow-display-none`.
        setToMinWidth() {
          // If context menu is visible, leave at full width.
          if (this.state.isContextMenuVisible) {
            return;
          }
          this.classList.add('overflow-display-none');
        }

        // For a button, the preferred width has the button displayed, so
        // removes the "overflow-display-none" class, checks if the parent
        // element fits in the window, and if not, adds back the
        // "overflow-display-none" class. Expects only to be called after an
        // initial setToMinWidth() call, and only if shouldBeShown() returns
        // true.
        expandUpToPreferredWidth() {
          // If context menu is visible, should already be at preferred width,
          // so do nothing.
          if (this.state.isContextMenuVisible) {
            return;
          }
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

        override willUpdate(changedProperties: PropertyValues<this>): void {
          super.willUpdate(changedProperties);
          if (changedProperties.has('state')) {
            const oldState = changedProperties.get('state');
            if (this.state.menuOpenToken !== undefined) {
              if (!oldState) {
                // Set `menuOpenToken_` to `state.menuOpenToken` if this is the
                // first state update - this is to prevent any issues restarting
                // the WebUI process after a crash. Doesn't matter if this is in
                // updated() or willUpdated(), just here because the else clause
                // needs to be here.
                this.menuOpenToken_ = this.state.menuOpenToken;
              } else if (this.state.menuOpenToken !== this.menuOpenToken_) {
                // If the browser hasn't caught up with `menuOpenToken_` yet,
                // keep `isContextMenuVisible` set to true. Since this modifies
                // `state`, needs to in willUpdate() rather than updated().
                this.state.isContextMenuVisible = true;
              }
            }
          }
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);
          if (changedProperties.has('state')) {
            const oldState = changedProperties.get('state');

            // When there's a context menu visible, control is not allowed to
            // overflow.
            if (this.state.isContextMenuVisible) {
              this.setToPreferredWidth();
            }

            // Need to run a layout if this is the first state update, if the
            // visibility of `this` changed, or if the visibility of the context
            // menu changed. Technically don't need to do a layout if the
            // context menu became visible when the button was not overflowed,
            // but that's a bit tricky to check.
            if (!oldState ||
                oldState.shouldBeShown !== this.state.shouldBeShown ||
                oldState.isContextMenuVisible !==
                    this.state.isContextMenuVisible) {
              this.fire('request-layout');
            }
          }
        }

        // Shows the context menu with the provided parameters, and makes the
        // button not overflow until the browser process has echoed back the
        // updated `menuOpenToken_` value. May only be used when the button's
        // State has `menuOpenToken` and `isContextMenuVisible` fields.
        showContextMenuAndPreventOverflow(
            menuType: ContextMenuType, sourceType: MenuSourceType) {
          // This is only supported if `state` has both `menuOpenToken` and
          // `isContextMenuVisible` fields.
          assert(this.state.menuOpenToken !== undefined);
          assert(this.state.isContextMenuVisible !== undefined);

          // `shouldBeShown` is currently handled entirely by subclasses. Since
          // we need a visible button to get the location when showing a context
          // menu, just give up if the button can't be shown. We could consider
          // making this class set `shouldBeShow` to true whenever it sets
          // `isContextMenuVisible` to true, to ensure buttons with context
          // menus are always shown, as opposed to only preventing them from
          // overflowing.
          if (!this.shouldBeShown()) {
            return;
          }

          // `>>> 0` is logically equivalent to casting to a uint32, which is
          // the type of the `menuOpenToken` field.
          this.menuOpenToken_ = (this.menuOpenToken_ + 1) >>> 0;
          this.state.isContextMenuVisible = true;
          // If this was overflowed, need to make visible and do a new layout
          // before getting the location to place the context menu.
          if (this.classList.contains('overflow-display-none')) {
            this.setToPreferredWidth();
            this.fire('layout-now');
          }

          BrowserProxyImpl.getInstance().toolbarUIHandler.showContextMenu(
              menuType, getContextMenuPosition(this), sourceType,
              this.menuOpenToken_);
        }
      }

      return OverflowableButtonMixin as T & Constructor<ResponsiveControl>&
          Constructor<OverflowableButton>;
    };
