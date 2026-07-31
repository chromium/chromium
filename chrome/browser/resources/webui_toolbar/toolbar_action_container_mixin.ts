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
  isDivider(key: string): boolean;
  onActionDragover(e: DragEvent): void;
  onActionDrop(e: DragEvent): void;
  moveItem(id: string, index: number): void;
  moveItemBy(id: string, delta: number): void;
  getMimeType(): string;
  getBroadcastChannelName(): string;
  childTagName: string;
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

        getKey(_state: T): string {
          assertNotReached();
        }

        moveItem(_id: string, _index: number) {
          assertNotReached();
        }

        moveItemBy(_id: string, _delta: number) {
          assertNotReached();
        }

        getMimeType(): string {
          assertNotReached();
        }

        getBroadcastChannelName(): string {
          assertNotReached();
        }

        get childTagName(): string {
          return assertNotReached();
        }

        isDivider(_key: string): boolean {
          return assertNotReached();
        }

        private draggedItemId_: string|null = null;
        private dragEnterCount_ = 0;
        private didDrop_ = false;
        private itemToFocusAfterUpdate_: string|null = null;

        // Channel to coordinate drag-and-drop state across different browser
        // windows.
        private dragChannel_: BroadcastChannel|null = null;

        // The action ID currently being dragged in *another* window, received
        // via the BroadcastChannel. Null if no external drag is active.
        private externallyDraggedItemId_: string|null = null;

        private mouseMoveListener_ = (e: MouseEvent) =>
            this.onWindowMouseMove_(e);
        private pointerDown_ = false;
        private deferredUpdate_ = false;
        private pointerDownListener_ = (e: PointerEvent) =>
            this.onHostPointerdown_(e);
        private pointerUpListener_ = () => this.onWindowPointerup_();

        override connectedCallback() {
          super.connectedCallback();
          // Initialize the BroadcastChannel for cross-window drag sync.
          this.dragChannel_ =
              new BroadcastChannel(this.getBroadcastChannelName());
          this.dragChannel_.onmessage = (e) => this.onDragChannelMessage_(e);
          // Mousemove and pointerup/cancel must be tracked globally to ensure
          // we clean up state even if the interaction ends outside the
          // container boundaries.
          window.addEventListener('mousemove', this.mouseMoveListener_);
          window.addEventListener('pointerup', this.pointerUpListener_);
          window.addEventListener('pointercancel', this.pointerUpListener_);
          // We only care about pointerdown events that originate inside the
          // container to start tracking user interaction for deferring updates.
          this.addEventListener('pointerdown', this.pointerDownListener_);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          if (this.dragChannel_) {
            this.dragChannel_.close();
            this.dragChannel_ = null;
          }
          window.removeEventListener('mousemove', this.mouseMoveListener_);
          this.removeEventListener('pointerdown', this.pointerDownListener_);
          window.removeEventListener('pointerup', this.pointerUpListener_);
          window.removeEventListener('pointercancel', this.pointerUpListener_);
        }

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          const changedPrivateProperties =
              changedProperties as Map<PropertyKey, unknown>;
          if (changedPrivateProperties.has('states')) {
            const oldState =
                changedPrivateProperties.get('states') as T[] | undefined;
            const newState = this.states;
            const oldIds = (oldState || []).map(s => this.getKey(s));
            const newIds = (newState || []).map(s => this.getKey(s));
            const orderChanged = oldIds.length !== newIds.length ||
                oldIds.some((id, index) => id !== newIds[index]);

            let shouldDefer = false;

            if (this.isDragging_) {
              if (orderChanged) {
                // State updates invalidate the layout index order, making
                // subsequent Mojo reorder calls unsafe. We must abort the drag
                // session.
                this.abortDrag_();
                // Do not defer, apply immediately.
                shouldDefer = false;
              } else {
                // Defer update.
                shouldDefer = true;
              }
            } else if (this.pointerDown_) {
              // Pointer down but not dragging: defer.
              shouldDefer = true;
            }

            if (shouldDefer) {
              this.deferredUpdate_ = true;
            } else {
              this.reconcileKeys();
              this.deferredUpdate_ = false;
            }
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

          // When a child action initiates dragging, we mark it locally as a
          // placeholder (making it invisible to act as a visual gap) and
          // broadcast the start event to sync other window toolbar instances.
          this.addEventListener('toolbar-action-drag-start', (e: Event) => {
            const customEvent = e as CustomEvent<{itemId: string}>;
            this.draggedItemId_ = customEvent.detail.itemId;
            if (this.deferredUpdate_) {
              this.abortDrag_();
              return;
            }
            if (this.dragChannel_) {
              this.dragChannel_.postMessage({
                type: 'drag-start',
                itemId: this.draggedItemId_,
              });
            }
            this.setPlaceholder_(this.draggedItemId_);
          });

          // When the drag session ends, we notify other windows to clear their
          // state, clear our local dragged ID, and restore the placeholder
          // element to normal.
          this.addEventListener('toolbar-action-drag-end', (e: Event) => {
            if (!this.draggedItemId_) {
              return;
            }
            const customEvent = e as CustomEvent<{
                                  itemId: string,
                                  dropEffect: string | undefined,
                                }>;
            const dropEffect = customEvent.detail?.dropEffect;
            const aborted = dropEffect !== 'move';

            if (this.dragChannel_) {
              this.dragChannel_.postMessage({
                type: 'drag-end',
                aborted: aborted,
              });
            }
            this.draggedItemId_ = null;
            this.didDrop_ = false;
            this.pointerDown_ = false;

            if (aborted) {
              this.clearPlaceholderAndRevert_();
            } else {
              this.clearPlaceholderFlagsOnly_();
            }
          });

          this.addEventListener(
              'toolbar-action-keyboard-reorder', (e: Event) => {
                const customEvent = e as CustomEvent<{itemId: string}>;
                this.itemToFocusAfterUpdate_ = customEvent.detail.itemId;
              });

          // We attach dragenter/dragleave/dragover/drop listeners to the host
          // element programmatically because we need to track when the drag
          // enters or leaves the boundaries of the entire container (using
          // dragEnterCount_). Child-level listeners cannot detect
          // container-level exits. Individual item dragover/drop handling is
          // declared on the children in the HTML template for declarative
          // clarity and easier target binding.
          this.addEventListener('dragenter', (e) => this.onHostDragenter_(e));
          this.addEventListener('dragleave', (e) => this.onHostDragleave_(e));
          this.addEventListener('dragover', (e) => this.onHostDragover_(e));
          this.addEventListener('drop', (e) => this.onHostDrop_(e));
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

        private get isDragging_(): boolean {
          return this.draggedItemId_ !== null ||
              this.externallyDraggedItemId_ !== null;
        }

        private getActiveAndSuffixStates_() {
          const suffixLength = this.getFixedSuffixLength_();
          const activeLength = this.keyedStates.length - suffixLength;
          return {
            active: this.keyedStates.slice(0, activeLength),
            suffix: this.keyedStates.slice(activeLength),
          };
        }

        private getActiveStates_(): Array<KeyedActionState<T>> {
          return this.getActiveAndSuffixStates_().active;
        }

        private getNonDividerActiveStates_(): Array<KeyedActionState<T>> {
          return this.getActiveStates_().filter(s => !this.isDivider(s.key));
        }

        private getPlaceholderIndex_(): number {
          return this.getNonDividerActiveStates_().findIndex(
              s => s.dragPlaceholder);
        }

        reconcileKeys() {
          this.deferredUpdate_ = false;
          const isInitial = this.isInitialUpdate(this.states);
          const currentKeyedStates = this.keyedStates || [];

          // 1. Map new mojo states to KeyedActionState (all active).
          const newKeyedStates: Array<KeyedActionState<T>> =
              this.states.map(state => {
                const key = this.getKey(state);
                const animateIn = !isInitial &&
                    !this.keyedStates.some(
                        old => old.key === key && !old.animateIn);
                const oldKeyedState =
                    currentKeyedStates.find(old => old.key === key);
                const dragPlaceholder =
                    oldKeyedState ? oldKeyedState.dragPlaceholder : undefined;
                return {key, state, animateIn, dragPlaceholder};
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

        override updated(changedProperties: PropertyValues<this>) {
          super.updated(changedProperties);

          // If a keyboard reorder event was captured, we need to restore focus
          // to the moved action button. We must wait for the DOM update to
          // complete (updateComplete promise) so that the elements are in their
          // new positions.
          if (this.itemToFocusAfterUpdate_ !== null) {
            const itemId = this.itemToFocusAfterUpdate_;
            this.itemToFocusAfterUpdate_ = null;

            this.updateComplete.then(() => {
              const el = this.shadowRoot.querySelector<HTMLElement>(
                  `${this.childTagName}[data-key="${itemId}"]`);
              if (el) {
                el.focus();
              }
            });
          }
        }

        private setPlaceholder_(itemId: string) {
          this.keyedStates = this.keyedStates.map(s => {
            if (s.key === itemId) {
              return {...s, dragPlaceholder: true};
            }
            return s;
          });
        }

        private clearPlaceholderFlagsOnly_() {
          this.keyedStates = this.keyedStates.map(s => {
            if (s.dragPlaceholder) {
              const {dragPlaceholder: _dragPlaceholder, ...rest} = s;
              return rest;
            }
            return s;
          });
        }

        private clearPlaceholderAndRevert_() {
          this.clearPlaceholderFlagsOnly_();
          this.reconcileKeys();
        }

        private abortDrag_() {
          if (this.draggedItemId_ !== null && this.dragChannel_) {
            this.dragChannel_.postMessage({
              type: 'drag-end',
              aborted: true,
            });
          }
          const draggedItemId =
              this.draggedItemId_ ?? this.externallyDraggedItemId_;
          if (draggedItemId !== null) {
            // HTML5 DND has no programmatic cancel method. Force-removing
            // the drag source element from the DOM tells the browser to
            // abort the native OS drag session. Lit will automatically
            // recreate the element in its new position during the
            // subsequent reconcileKeys update.
            const el = this.shadowRoot.querySelector(
                `${this.childTagName}[data-key="${draggedItemId}"]`);
            if (el) {
              el.remove();
            }
          }
          this.draggedItemId_ = null;
          this.externallyDraggedItemId_ = null;
          this.clearPlaceholderFlagsOnly_();
        }

        // Handles drag state updates broadcasted from other toolbar instances,
        // allowing us to sync and track cross-window drags.
        private onDragChannelMessage_(e: MessageEvent) {
          if (e.data.type === 'drag-start') {
            this.externallyDraggedItemId_ = e.data.itemId;
            // Guard against race: if the drag already entered this window
            // before we received the broadcast, apply the placeholder now.
            if (this.dragEnterCount_ > 0) {
              this.setPlaceholder_(e.data.itemId);
            }
          } else if (e.data.type === 'drag-end') {
            this.externallyDraggedItemId_ = null;
            if (e.data.aborted) {
              this.clearPlaceholderAndRevert_();
            } else {
              this.clearPlaceholderFlagsOnly_();
            }
          }
        }

        private onHostPointerdown_(e: PointerEvent) {
          if (!e.isPrimary) {
            return;
          }
          const item =
              e.composedPath().find(
                  el => el instanceof HTMLElement &&
                      el.localName === this.childTagName) as HTMLElement |
              undefined;
          if (item) {
            // Track pointerdown on child items to delay state updates that
            // could cause reordering, avoiding the button moving from under
            // the pointer.
            this.pointerDown_ = true;
          }
        }

        private onWindowPointerup_() {
          this.pointerDown_ = false;
          this.applyDeferredUpdateIfAny_();
        }

        // Mojo updates are deferred during a drag and applied later by this
        // function.
        private applyDeferredUpdateIfAny_() {
          if (this.pointerDown_ || this.isDragging_) {
            return;
          }
          if (this.deferredUpdate_) {
            this.deferredUpdate_ = false;
            this.reconcileKeys();
          }
        }

        // A fallback listener to guard against the browser or OS failing to
        // fire the native 'dragend' event (e.g., if the drag is aborted over a
        // non-chrome window). During an active browser drag session, standard
        // 'mousemove' events are suppressed. If we receive a 'mousemove' while
        // we still think a drag is active, we know the drag must have ended
        // externally, so we force state cleanup.
        private onWindowMouseMove_(e: MouseEvent) {
          if (e.buttons !== 0) {
            return;
          }
          if (this.draggedItemId_ !== null ||
              this.externallyDraggedItemId_ !== null) {
            const isSource = this.draggedItemId_ !== null;
            this.draggedItemId_ = null;
            this.externallyDraggedItemId_ = null;
            this.clearPlaceholderAndRevert_();
            if (isSource && this.dragChannel_) {
              this.dragChannel_.postMessage({
                type: 'drag-end',
                aborted: true,
              });
            }
          }
        }

        // Gets the length of non-draggable extensions.
        private getFixedSuffixLength_(): number {
          let suffixLength = 0;
          for (let i = this.keyedStates.length - 1; i >= 0; i--) {
            const state = this.keyedStates[i]!;
            const el = this.shadowRoot.querySelector<HTMLElement&{
              isDraggable?: () => boolean,
            }>(`${this.childTagName}[data-key="${state.key}"]`);
            if (el && el.isDraggable && !el.isDraggable()) {
              suffixLength++;
            } else {
              break;
            }
          }
          return suffixLength;
        }

        onActionDragover(e: DragEvent) {
          if (!e.dataTransfer) {
            return;
          }
          if (!e.dataTransfer.types.includes(this.getMimeType())) {
            return;
          }
          // Identify which action is being dragged, checking first if it is a
          // local drag (draggedItemId_) or a cross-window drag
          // (externallyDraggedItemId_).
          const draggedItemId =
              this.draggedItemId_ ?? this.externallyDraggedItemId_;
          if (draggedItemId === null) {
            return;
          }
          e.preventDefault();
          e.dataTransfer.dropEffect = 'move';
          const target = e.currentTarget as HTMLElement;

          const key = target?.dataset['key'];
          const overState = this.keyedStates.find(s => s.key === key);
          if (!overState || this.isDivider(overState.key)) {
            return;
          }
          // If the user hovers over the dragged action itself, do nothing. It
          // is already marked as a placeholder on dragstart or host dragenter.
          if (draggedItemId === overState.key) {
            return;
          }

          // Re-order the actions to show what would happen on drop.

          const {active: activeStates, suffix: suffixStates} =
              this.getActiveAndSuffixStates_();

          const fromIndex =
              activeStates.findIndex(s => s.key === draggedItemId);
          if (fromIndex === -1) {
            return;
          }

          let toIndex = activeStates.findIndex(s => s.key === overState.key);
          if (toIndex === -1) {
            toIndex = activeStates.length;
          }

          if (fromIndex !== toIndex) {
            const newActive = [...activeStates];
            const [moved] = newActive.splice(fromIndex, 1);
            if (moved) {
              const insertIndex = Math.min(toIndex, newActive.length);
              newActive.splice(insertIndex, 0, moved);
            }
            this.keyedStates = [...newActive, ...suffixStates];
          }
        }

        onActionDrop(e: DragEvent) {
          if (!e.dataTransfer ||
              !e.dataTransfer.types.includes(this.getMimeType())) {
            return;
          }

          try {
            const data = JSON.parse(e.dataTransfer.getData(this.getMimeType()));
            const itemId = data.itemId;
            if (itemId === undefined || itemId === null) {
              return;
            }

            // Guard: Dropped action ID must match what we are tracking as
            // currently dragged locally or externally.
            if (itemId !== this.draggedItemId_ &&
                itemId !== this.externallyDraggedItemId_) {
              return;
            }

            e.preventDefault();
            e.stopPropagation();

            this.dragEnterCount_ = 0;
            this.didDrop_ = true;

            const placeholderIdx = this.getPlaceholderIndex_();
            const targetIndex = placeholderIdx !== -1 ?
                placeholderIdx :
                this.getNonDividerActiveStates_().length;

            this.moveItem(itemId, targetIndex);
          } catch (_err) {
            this.draggedItemId_ = null;
          }
        }

        private onHostDragenter_(e: DragEvent) {
          if (!e.dataTransfer ||
              !e.dataTransfer.types.includes(this.getMimeType())) {
            return;
          }
          this.dragEnterCount_++;
          if (this.dragEnterCount_ === 1) {
            const draggedItemId =
                this.draggedItemId_ ?? this.externallyDraggedItemId_;
            if (draggedItemId !== null) {
              this.setPlaceholder_(draggedItemId);
            }
          }
        }

        private onHostDragleave_(e: DragEvent) {
          if (!e.dataTransfer ||
              !e.dataTransfer.types.includes(this.getMimeType())) {
            return;
          }
          this.dragEnterCount_--;
          if (this.dragEnterCount_ === 0) {
            this.reconcileKeys();
          }
        }

        private onHostDragover_(e: DragEvent) {
          if (e.dataTransfer &&
              e.dataTransfer.types.includes(this.getMimeType())) {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
          }
        }

        private onHostDrop_(e: DragEvent) {
          this.dragEnterCount_ = 0;
          if (this.didDrop_) {
            return;
          }

          if (!e.dataTransfer ||
              !e.dataTransfer.types.includes(this.getMimeType())) {
            this.clearPlaceholderAndRevert_();
            return;
          }

          try {
            const data = JSON.parse(e.dataTransfer.getData(this.getMimeType()));
            const itemId = data.itemId;
            if (itemId === undefined || itemId === null) {
              this.clearPlaceholderAndRevert_();
              return;
            }

            const draggedItemId =
                this.draggedItemId_ ?? this.externallyDraggedItemId_;
            if (itemId !== draggedItemId) {
              this.clearPlaceholderAndRevert_();
              return;
            }

            if (draggedItemId !== null) {
              const placeholderIdx = this.getPlaceholderIndex_();
              if (placeholderIdx !== -1) {
                e.preventDefault();
                this.didDrop_ = true;
                this.moveItem(draggedItemId, placeholderIdx);
                return;
              }
            }
          } catch (_err) {
            // Ignore parse errors
          }

          this.clearPlaceholderAndRevert_();
        }
      }

      return ToolbarActionContainerMixin;
    };
