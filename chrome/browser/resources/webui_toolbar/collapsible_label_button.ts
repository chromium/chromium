// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {OverflowMenuItem} from '/shared/toolbar_ui_api.mojom-webui.js';

import type {ToolbarAppElement} from './app.js';
import type {ResponsiveControl} from './responsive_control.js';

type Constructor<T> = new (...args: any[]) => T;

export declare class CollapsibleLabelButton extends EventTarget implements
    ResponsiveControl {
  collapsed: boolean;
  shouldBeShown(): boolean;
  setToMinWidth(): void;
  setToPreferredWidth(): void;
  expandUpToPreferredWidth(): void;
  controlsToAddToOverflowMenu(): OverflowMenuItem[];
  protected hasLabel_(): boolean;
  protected shouldShowLabel_(): boolean;
}

/**
 * A mixin for toolbar chip buttons (such as the Glic button and App Menu
 * button) that collapse their text label down to an icon-only button when
 * toolbar space is constrained, without ever disappearing into the overflow
 * menu.
 *
 * Expects the host CrLitElement to contain a `#button` child (typically a
 * `<toolbar-chip-button id="button">`). Subclasses should override
 * `hasLabel_()` (and `shouldBeShown()` if the button can be hidden) and
 * dispatch a `request-layout` event when their label or visibility state
 * changes.
 */
export const CollapsibleLabelButtonMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<CollapsibleLabelButton> => {
      class CollapsibleLabelButtonMixin extends superClass {
        static get properties() {
          return {
            collapsed: {
              type: Boolean,
              reflect: true,
            },
          };
        }

        accessor collapsed: boolean = false;

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);
          if (this.collapsed && !this.hasLabel_()) {
            this.setCollapsed_(false);
          } else {
            this.toggleAttribute('has-label', this.shouldShowLabel_());
          }
        }

        protected hasLabel_(): boolean {
          return false;
        }

        protected shouldShowLabel_(): boolean {
          return this.hasLabel_() && !this.collapsed;
        }

        shouldBeShown(): boolean {
          return true;
        }

        setToMinWidth() {
          this.setCollapsed_(this.hasLabel_());
        }

        setToPreferredWidth() {
          this.setCollapsed_(false);
        }

        expandUpToPreferredWidth() {
          if (!this.hasLabel_()) {
            return;
          }
          this.setToPreferredWidth();
          const shadowRoot = this.getRootNode() as ShadowRoot;
          const toolbarApp = shadowRoot?.host as ToolbarAppElement | undefined;
          if (toolbarApp && toolbarApp.getAvailableWidth() < 0) {
            this.setToMinWidth();
          }
        }

        controlsToAddToOverflowMenu(): OverflowMenuItem[] {
          return [];
        }

        private setCollapsed_(collapsed: boolean) {
          if (this.collapsed === collapsed) {
            return;
          }
          this.collapsed = collapsed;
          this.toggleAttribute('collapsed', collapsed);
          const showLabel = this.shouldShowLabel_();
          this.toggleAttribute('has-label', showLabel);
          const innerButton =
              this.shadowRoot?.querySelector<HTMLElement>('#button');
          // Toggle `has-label` directly rather than relying on Lit's
          // asynchronous data binding (`shouldShowLabel_()`) so
          // `toolbar-chip-button` updates its padding immediately for
          // synchronous layout width measurements in
          // `ToolbarAppElement.layoutResponsiveControls()`.
          innerButton?.toggleAttribute('has-label', showLabel);
        }
      }

      return CollapsibleLabelButtonMixin as unknown as T &
          Constructor<CollapsibleLabelButton>;
    };
