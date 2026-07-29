// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

// State pushed to Lit template for rendering.
export interface KeyedActionState<T> {
  // Key so repeat directive can maintain consistent mapping between this
  // particular state and the Lit element.
  key: string;
  // Most of the state of the Lit element.
  state: T;
  // Is this element sliding out (i.e. exiting)?
  // If true, this instance will be deleted from `keyedStates` when the
  // slide-out animation completes by `onTransitionDone_()`.
  exiting?: boolean;
  // Should this element animate in (i.e. slide in)?
  animateIn?: boolean;
  // Is this element currently being dragged (rendering as gap/placeholder)?
  dragPlaceholder?: boolean;
}

type Constructor<T> = new (...args: any[]) => T;

// Class to track whether the user desires animations to be shown. This is
// exposed via `AnimationTracker.showAnimations`. For testing, `showAnimations`
// can be directly set, then `resetForTesting()` can be used to reset it during
// test teardown.
export class AnimationTracker {
  private static reducedMotion_: MediaQueryList =
      window.matchMedia('(prefers-reduced-motion: reduce)');

  static showAnimations: boolean = !AnimationTracker.reducedMotion_.matches;

  static {
    AnimationTracker.reducedMotion_.addEventListener('change', () => {
      AnimationTracker.showAnimations =
          !AnimationTracker.reducedMotion_.matches;
    });
  }

  // If tests directly modify `showAnimations`, the tests should call this
  // method during teardown to reset it.
  static resetForTesting() {
    AnimationTracker.showAnimations = !AnimationTracker.reducedMotion_.matches;
  }
}

export interface ToolbarActionContainerMixinInterface<T> {
  states: T[];
  keyedStates: Array<KeyedActionState<T>>;
  getKey(state: T): string;
  isInitialUpdate(newStates: T[]): boolean;
  allExiting(): boolean;
  animateInDivider(): boolean;
  reconcileKeys(): void;
}

/**
 * A mixin for WebUI toolbar icon container elements (such as pinned toolbar
 * actions and extensions) that manage a list of dynamic items with slide-in
 * and slide-out animations.
 *
 * It reconciles new state arrays (`state`) against internal keyed items
 * (`keyedStates`), automatically setting animation flags (`animateIn` for
 * newly added items, and `exiting` for removed items) while keeping exiting
 * items in the DOM until their CSS transitions complete.
 *
 * Usage:
 * 1. Mix in `ToolbarActionContainerMixin<T>` where `T` is your item state type.
 * 2. Implement `getKey(state: T): string` to return a unique key for each item.
 * 3. Optionally override `isInitialUpdate(newStates: T[]): boolean` to suppress
 *    slide-in animations on initial load.
 * 4. In your Lit template (`.html.ts`), iterate over `this.keyedStates` using
 *    the Lit `repeat()` directive, applying `animate-in` and `exiting` CSS
 *    classes based on `keyedState.animateIn` and `keyedState.exiting`.
 */
export const ToolbarActionContainerMixin =
    <T, BaseClass extends Constructor<CrLitElement>>(
        superClass: BaseClass, initialState: T[]): BaseClass&
    Constructor<ToolbarActionContainerMixinInterface<T>> => {
      class ToolbarActionContainerMixin extends superClass implements
          ToolbarActionContainerMixinInterface<T> {
        static get properties() {
          return {
            states: {type: Array},
            keyedStates: {type: Array},
          };
        }

        accessor states: T[] = initialState;

        // Internal reactive state that includes exiting items.
        accessor keyedStates: Array<KeyedActionState<T>> = [];

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          if (changedProperties.has('states')) {
            this.reconcileKeys();
          }
        }

        override firstUpdated(changedProperties: PropertyValues<this>) {
          super.firstUpdated(changedProperties);
          // Add listener to shadow root to catch bubbled transitionend and
          // transitioncancel events.
          this.shadowRoot.addEventListener(
              'transitionend',
              e => this.onTransitionDone_(e as TransitionEvent));
          this.shadowRoot.addEventListener(
              'transitioncancel',
              e => this.onTransitionDone_(e as TransitionEvent));
        }

        // Child classes must override this to return a unique key for each
        // item.
        getKey(_state: T): string {
          assertNotReached();
        }

        // Child classes can override this to compute when `_newStates`
        // indicates an initial state, e.g. when you create a new window.
        // Slide-in animations are not performed for initial states.
        isInitialUpdate(_newStates: T[]): boolean {
          return this.keyedStates.length === 0;
        }

        allExiting(): boolean {
          return this.keyedStates.length > 0 &&
              this.keyedStates.every(s => s.exiting);
        }

        animateInDivider(): boolean {
          return this.keyedStates.length > 0 &&
              this.keyedStates.every(s => s.animateIn);
        }

        reconcileKeys() {
          const isInitial = this.isInitialUpdate(this.states);

          // 1. Map new mojo states to KeyedActionState (all active).
          const newKeyedStates: Array<KeyedActionState<T>> =
              this.states.map(state => {
                const key = this.getKey(state);
                const animateIn = !isInitial &&
                    !this.keyedStates.some(
                        old => old.key === key && !old.animateIn);
                return {key, state, animateIn};
              });

          // 2. Find which keys were in the old `keyedStates` but are not in
          // `newKeyedStates`. These are the ones that are "sliding-out".
          const newKeys = new Set(newKeyedStates.map(s => s.key));
          const missingOldStates =
              this.keyedStates.filter(s => !newKeys.has(s.key));

          // 3. Re-insert "sliding-out" states (marked as exiting) into their
          // old positions in `keyedStates` so they'll be rendered while
          // "sliding-out" if animations are enabled.
          if (AnimationTracker.showAnimations) {
            // Sort missing states by their original index to preserve order
            // during insertion
            const oldKeyToIndex =
                new Map(this.keyedStates.map((s, i) => [s.key, i]));
            missingOldStates.sort(
                (a, b) => oldKeyToIndex.get(a.key)! - oldKeyToIndex.get(b.key)!,
            );

            // Insert them back with `exiting` set to true.
            for (const missing of missingOldStates) {
              const exitingState = {...missing, exiting: true};
              const originalIndex = oldKeyToIndex.get(missing.key)!;
              const insertIndex =
                  Math.min(originalIndex, newKeyedStates.length);
              newKeyedStates.splice(insertIndex, 0, exitingState);
            }
          }

          this.keyedStates = newKeyedStates;
          this.updateVisibility_();

          // If the layout engine has already forced the exiting elements to 0
          // width (preempting the transition), or if animations are disabled,
          // remove them immediately.
          this.updateComplete.then(() => {
            for (const el of this.shadowRoot.querySelectorAll<HTMLElement>(
                     '.exiting')) {
              // If it's already 0px wide, it won't transition.
              if (el.getBoundingClientRect().width === 0) {
                const key = el.dataset['key'];
                if (key) {
                  // Extensions can only be unpinned one-at-a-time, so
                  // one-at-a-time removal should be sufficiently performant.
                  this.keyedStates =
                      this.keyedStates.filter(s => s.key !== key);
                }
              }
            }
            this.updateVisibility_();
          });
        }

        // When an element finishes "sliding-out", remove it from
        // `keyedStates`.
        private onTransitionDone_(e: TransitionEvent) {
          // We only care about the width transition to trigger removal
          if (e.propertyName !== 'width') {
            return;
          }

          const target = e.target as HTMLElement;
          if (!target.classList.contains('exiting')) {
            return;
          }

          const key = target.dataset['key'];
          if (!key) {
            return;
          }

          // Remove the finished item (automatically triggers update)
          this.keyedStates = this.keyedStates.filter(s => s.key !== key);
          this.updateVisibility_();
        }

        private updateVisibility_() {
          this.hidden = !this.keyedStates || this.keyedStates.length === 0;
        }
      }

      return ToolbarActionContainerMixin;
    };
