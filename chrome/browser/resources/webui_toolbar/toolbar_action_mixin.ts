// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_interface.js';

import type {HelpBubbleAnchor} from './toolbar_button.js';
import {HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';

type Constructor<T> = new (...args: any[]) => T;

export interface ToolbarActionMixinInterface<T> extends
    HelpBubbleMixinInterface, HelpBubbleAnchor {
  state: T;
  // Set if the TrackedElementManager indicates the element should be
  // highlighted.
  trackedHighlighted: boolean;
  getElementId(state: T): string|undefined;
  getSecondaryElementId(): string|undefined;
}

/**
 * A mixin for individual WebUI toolbar action buttons (such as pinned toolbar
 * actions and extensions).
 *
 * It manages help bubble registration and tracking, waiting for any entrance
 * or movement animations to finish before registering help bubbles so they
 * anchor at the button's final DOM location. It also handles cleanup on
 * disconnect and updating bubble registration when the element's identifier
 * changes.
 */
export const ToolbarActionMixin =
    <T, BaseClass extends Constructor<CrLitElement>>(
        superClass: BaseClass, initialState: T): BaseClass&
    Constructor<ToolbarActionMixinInterface<T>> => {
      const superClassBase = HelpBubbleAnchorMixin(superClass);

      class ToolbarActionMixin extends superClassBase implements
          ToolbarActionMixinInterface<T> {
        static get properties() {
          return {
            state: {type: Object},
            trackedHighlighted: {type: Boolean},
          };
        }

        accessor state: T = initialState;
        accessor trackedHighlighted: boolean = false;

        private registerHelpBubbleController_: AbortController|null = null;

        override disconnectedCallback() {
          super.disconnectedCallback();
          if (this.registerHelpBubbleController_) {
            this.registerHelpBubbleController_.abort();
            this.registerHelpBubbleController_ = null;
          }
        }

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          if (changedProperties.has('state')) {
            const oldState = changedProperties.get('state');
            const oldId = oldState ? this.getElementId(oldState) : undefined;
            const newId =
                this.state ? this.getElementId(this.state) : undefined;

            if (oldId !== newId) {
              if (this.registerHelpBubbleController_) {
                this.registerHelpBubbleController_.abort();
                this.registerHelpBubbleController_ = null;
              }
              if (oldId) {
                this.unregisterHelpBubble(oldId);
              }
              if (newId) {
                this.registerHelpBubble_(newId);
              }
            }
          }
        }

        /**
         * Returns the primary ElementIdentifier for help bubbles, or null/empty
         * if this item does not support help bubbles.
         */
        getElementId(_state: T): string|undefined {
          return undefined;
        }

        /**
         * Optionally returns a secondary identifier for help bubbles (e.g.
         * 'ext:id').
         */
        getSecondaryElementId(): string|undefined {
          return undefined;
        }

        private async registerHelpBubble_(newId: string) {
          this.registerHelpBubbleController_ = new AbortController();
          const signal = this.registerHelpBubbleController_.signal;

          const animations = this.getAnimations().filter(animation => {
            const timing = animation.effect?.getTiming();
            if (!timing) {
              return true;
            }
            // Ignore infinite animations (e.g. pulsing for IPH).
            return timing.iterations !== Infinity &&
                timing.duration !== Infinity;
          });

          // Wait for any animations to complete, so button is in final
          // location.
          if (animations.length > 0) {
            try {
              await Promise.all(animations.map(a => a.finished));
            } catch (e) {
              // Ignore animation cancellation.
            }
          }

          if (signal.aborted) {
            return;
          }

          this.registerHelpBubble(newId, this, {
            secondaryId: this.getSecondaryElementId(),
            onHighlightChanged: (highlighted: boolean) => {
              this.trackedHighlighted = highlighted;
            },
            onHelpBubbleShown: () => setHasHelpBubble(this, true),
            onHelpBubbleHidden: () => setHasHelpBubble(this, false),
          });
          this.registerHelpBubbleController_ = null;
        }
      }

      return ToolbarActionMixin;
    };
