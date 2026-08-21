// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {AnimationTracker} from '/shared/animation_tracker.js';

// State pushed to Lit template for rendering.
export interface KeyedActionState<T> {
  // Key so repeat directive can maintain consistent mapping between this
  // particular state and the Lit element.
  key: string;
  // Most of the state of the Lit element.
  state: T;
  // Is this element sliding in?
  // If true, @starting-style will apply to transition it into view, and
  // animateIn will be reset to false on transitionend/transitioncancel.
  animateIn?: boolean;
  // Is this element sliding out (i.e. exiting)?
  // If true, this instance will be deleted from `keyedStates` when the
  // slide-out animation completes by `onTransitionDone_()`.
  exiting?: boolean;
  // Is this element currently being dragged (rendering as gap/placeholder)?
  dragPlaceholder?: boolean;
}

type Constructor<T> = new (...args: any[]) => T;

export interface ToolbarActionContainerMixinInterface<T> {
  states: T[];
  keyedStates: Array<KeyedActionState<T>>;
  getKey(state: T): string;
  isInitialUpdate(newStates: T[]): boolean;
  allExiting(): boolean;
  animateInDivider(): boolean;
  reconcileKeys(): void;
  isDraggable(state: T, index: number): boolean;
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
 * It reconciles new state arrays (`states`) against internal keyed items
 * (`keyedStates`), automatically setting the `exiting` flag for removed items
 * while keeping exiting items in the DOM until their CSS transitions complete.
 *
 * Usage:
 * 1. Mix in `ToolbarActionContainerMixin<T>` where `T` is your item state type.
 * 2. Implement `getKey(state: T): string` to return a unique key for each item.
 * 3. Implement `isDraggable(state: T, index: number): boolean` to specify
 *    which items are draggable. Non-draggable items must be positioned
 *    consecutively at the very end of the list, forming a fixed undraggable
 *    suffix that drag-and-drop operations cannot reorder or enter.
 * 4. Optionally override `isInitialUpdate(newStates: T[]): boolean` to suppress
 *    slide-in animations on initial load.
 * 5. In your Lit template (`.html.ts`), iterate over `this.keyedStates` using
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

        isDraggable(_state: T, _index: number): boolean {
          return assertNotReached();
        }

        /**
         * The ID of the item currently being dragged locally. Null if no local
         * drag is active.
         */
        private draggedItemId_: string|null = null;

        /**
         * True if the drag is currently over the host element.
         */
        private isDragOverHost_ = false;

        /**
         * True if a drop event occurred in this container, preventing revert
         * on dragend.
         */
        private didDrop_ = false;

        /** The ID of the item to focus after the next render update. */
        private itemToFocusAfterUpdate_: string|null = null;

        /**
         * The number of draggable items in the container. Draggable items are
         * always positioned consecutively at the beginning of the list.
         */
        draggableItemsCount: number = 0;

        // Channel to coordinate drag-and-drop state across different browser
        // windows.
        private dragChannel_: BroadcastChannel|null = null;

        // The action ID currently being dragged in *another* window, received
        // via the BroadcastChannel. Null if no external drag is active.
        private externallyDraggedItemId_: string|null = null;

        private mouseMoveListener_ = (e: MouseEvent) =>
            this.onWindowMouseMove_(e);


        override connectedCallback() {
          super.connectedCallback();
          // Disable transitions initially to prevent entry animations on first
          // render.
          this.classList.add('initial-load');
          // Initialize the BroadcastChannel for cross-window drag sync.
          this.dragChannel_ =
              new BroadcastChannel(this.getBroadcastChannelName());
          this.dragChannel_.onmessage = (e) => this.onDragChannelMessage_(e);
          // Mousemove must be tracked globally to ensure we clean up state even
          // if the interaction ends outside the container boundaries.
          window.addEventListener('mousemove', this.mouseMoveListener_);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          if (this.dragChannel_) {
            this.dragChannel_.close();
            this.dragChannel_ = null;
          }
          window.removeEventListener('mousemove', this.mouseMoveListener_);
        }

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          const changedPrivateProperties =
              changedProperties as Map<PropertyKey, unknown>;
          if (changedPrivateProperties.has('states')) {
            const newIds = (this.states || []).map(s => this.getKey(s));

            if (this.isDragging_ && this.draggedItemId_ !== null) {
              if (!newIds.includes(this.draggedItemId_)) {
                this.abortDrag_();
              }
            }
            this.reconcileKeys();
          }

          if (changedProperties.has('keyedStates')) {
            let draggableCount = 0;
            for (let i = this.keyedStates.length - 1; i >= 0; i--) {
              if (this.isDraggable(this.keyedStates[i]!.state, i)) {
                draggableCount = i + 1;
                break;
              }
            }
            this.draggableItemsCount = draggableCount;
          }
        }

        override firstUpdated(changedProperties: PropertyValues<this>) {
          super.firstUpdated(changedProperties);
          // Catch bubbled transition events to remove exiting items from the
          // DOM once their collapse transition completes.
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
            this.isDragOverHost_ = true;
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

        private getSuffixStates_(): Array<KeyedActionState<T>> {
          return this.keyedStates.slice(this.draggableItemsCount);
        }

        private getActiveStates_(): Array<KeyedActionState<T>> {
          return this.keyedStates.slice(0, this.draggableItemsCount);
        }

        private getPlaceholderIndex_(): number {
          return this.getActiveStates_().findIndex(s => s.dragPlaceholder);
        }

        private mapStates_(
            states: T[], isInitial: boolean,
            currentKeyedStates: Array<KeyedActionState<T>>):
            Array<KeyedActionState<T>> {
          return states.map(
              state => {
                const key = this.getKey(state);
                // Animate in if this is not the initial load, animations are
                // enabled, and the item is either not in `currentKeyedStates`
                // or already animating. If it was already in
                // `currentKeyedStates` and not animating, we use the
                // transition to smoothly change to its desired width.
                const animateIn = !isInitial &&
                    AnimationTracker.showAnimations &&
                    !currentKeyedStates.some(
                        old => old.key === key && !old.animateIn);
                const oldKeyedState =
                    currentKeyedStates.find(old => old.key === key);
                const dragPlaceholder =
                    oldKeyedState ? oldKeyedState.dragPlaceholder : undefined;
                return {key, state, animateIn, dragPlaceholder};
              });
        }

        reconcileKeys() {
          // 1. Map new mojo states to KeyedActionState (all active).
          const isInitial = this.isInitialUpdate(this.states);
          if (!isInitial) {
            // Enable transitions for subsequent updates after the initial load.
            this.classList.remove('initial-load');
          }
          const currentKeyedStates = this.keyedStates || [];

          let newKeyedStates: Array<KeyedActionState<T>>;

          const draggedId =
              this.draggedItemId_ ?? this.externallyDraggedItemId_;

          if (this.isDragging_ && this.isDragOverHost_ && draggedId !== null) {
            const localIndex =
                this.keyedStates.findIndex(s => s.key === draggedId);

            if (localIndex !== -1) {
              const oldDraggedKeyedState = this.keyedStates[localIndex]!;
              const mojoDraggedState =
                  this.states.find(s => this.getKey(s) === draggedId);
              const state = mojoDraggedState ?? oldDraggedKeyedState.state;
              const draggedKeyedState = {
                ...oldDraggedKeyedState,
                state,
              };

              const statesWithoutDragged =
                  this.states.filter(s => this.getKey(s) !== draggedId);

              newKeyedStates = this.mapStates_(
                  statesWithoutDragged, isInitial, currentKeyedStates);

              const insertIndex = Math.min(localIndex, newKeyedStates.length);
              newKeyedStates.splice(insertIndex, 0, draggedKeyedState);
            } else {
              newKeyedStates =
                  this.mapStates_(this.states, isInitial, currentKeyedStates);
            }
          } else {
            newKeyedStates =
                this.mapStates_(this.states, isInitial, currentKeyedStates);
          }

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
            missingOldStates.sort(
                (a, b) => this.keyedStates.findIndex(s => s.key === a.key) -
                    this.keyedStates.findIndex(s => s.key === b.key),
            );

            // Insert them back with `exiting` set to true.
            for (const missing of missingOldStates) {
              const exitingState = {
                ...missing,
                exiting: true,
                animateIn: false,
              };
              const originalIndex =
                  this.keyedStates.findIndex(s => s.key === missing.key);
              const insertIndex =
                  Math.min(originalIndex, newKeyedStates.length);
              newKeyedStates.splice(insertIndex, 0, exitingState);
            }
          }

          this.keyedStates = newKeyedStates;
          this.updateVisibility_();

          // If CSS forces the exiting elements to 0 width immediately (e.g. if
          // animations are disabled), remove them synchronously once the update
          // completes. getBoundingClientRect() forces layout so we get the
          // transitioning width if a transition is running.
          this.updateComplete.then(() => {
            const exitingElements =
                this.shadowRoot.querySelectorAll<HTMLElement>('.exiting');
            for (const el of exitingElements) {
              const width = el.getBoundingClientRect().width;
              // If it's already 0px wide, it won't have a transitionend event.
              if (width === 0) {
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

        // Handler for transition events. This is called for all transition
        // events bubbled up to the container. It handles cleanup for both:
        // 1. Sliding-out elements (exiting): removes them once they have
        // collapsed
        //    to 0 width.
        // 2. Sliding-in elements (animateIn): clears the animateIn flag once
        // the
        //    entrance width transition finishes, so subsequent DOM moves don't
        //    re-trigger @starting-style.
        private onTransitionDone_(e: TransitionEvent) {
          // We only care about the width transition to trigger removal /
          // animateIn cleanup.
          if (e.propertyName !== 'width') {
            return;
          }

          const target = e.target as HTMLElement;
          const key = target.dataset['key'];
          if (!key) {
            return;
          }

          if (target.classList.contains('exiting')) {
            // If the transition was cancelled, or even if it ended, only remove
            // the element if it reached 0 width. If cancelled because a new
            // transition started (e.g. reversing), width will be non-zero.
            if (target.getBoundingClientRect().width !== 0) {
              return;
            }

            // Remove the finished item (automatically triggers update)
            this.keyedStates = this.keyedStates.filter(s => s.key !== key);
            this.updateVisibility_();
            return;
          }

          if (target.classList.contains('animate-in')) {
            const stateToUpdate = this.keyedStates.find(s => s.key === key);
            if (!stateToUpdate || !stateToUpdate.animateIn) {
              return;
            }

            this.keyedStates = this.keyedStates.map(s => {
              if (s.key === key) {
                return {...s, animateIn: false};
              }
              return s;
            });
          }
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
          if (this.draggedItemId_ !== null) {
            const el = this.shadowRoot.querySelector<HTMLElement>(
                `${this.childTagName}[data-key="${this.draggedItemId_}"]`);
            if (el) {
              el.style.display = 'none';
            }
            if (this.dragChannel_) {
              this.dragChannel_.postMessage({
                type: 'drag-end',
                aborted: true,
              });
            }
            // Remove the dragged item from keyedStates so reconcileKeys() does
            // not retain it as an exiting state. Because display is set to
            // 'none', no CSS transition will run or fire transitionend to clean
            // it up.
            this.keyedStates =
                this.keyedStates.filter(s => s.key !== this.draggedItemId_);
          }
          this.draggedItemId_ = null;
          this.externallyDraggedItemId_ = null;
          this.clearPlaceholderAndRevert_();
        }

        // Handles drag state updates broadcasted from other toolbar instances,
        // allowing us to sync and track cross-window drags.
        private onDragChannelMessage_(e: MessageEvent) {
          if (e.data.type === 'drag-start') {
            this.externallyDraggedItemId_ = e.data.itemId;
            // Guard against race: if the drag already entered this window
            // before we received the broadcast, apply the placeholder now.
            if (this.isDragOverHost_) {
              this.setPlaceholder_(e.data.itemId);
            }
          } else if (e.data.type === 'drag-end') {
            this.externallyDraggedItemId_ = null;
            this.isDragOverHost_ = false;
            if (e.data.aborted) {
              this.clearPlaceholderAndRevert_();
            } else {
              this.clearPlaceholderFlagsOnly_();
            }
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
          if (!overState) {
            return;
          }
          // If the user hovers over the dragged action itself, do nothing. It
          // is already marked as a placeholder on dragstart or host dragenter.
          if (draggedItemId === overState.key) {
            return;
          }

          // Re-order the actions to show what would happen on drop.

          const activeStates = this.getActiveStates_();
          const suffixStates = this.getSuffixStates_();

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
              if (fromIndex !== insertIndex) {
                this.keyedStates = [...newActive, ...suffixStates];
              }
            }
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

            this.didDrop_ = true;

            const placeholderIdx = this.getPlaceholderIndex_();
            const targetIndex = placeholderIdx !== -1 ?
                placeholderIdx :
                this.draggableItemsCount;

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
          if (e.relatedTarget && this.contains(e.relatedTarget as Node)) {
            return;
          }
          this.isDragOverHost_ = true;
          const draggedItemId =
              this.draggedItemId_ ?? this.externallyDraggedItemId_;
          if (draggedItemId !== null) {
            this.setPlaceholder_(draggedItemId);
          }
        }

        private onHostDragleave_(e: DragEvent) {
          if (!e.dataTransfer ||
              !e.dataTransfer.types.includes(this.getMimeType())) {
            return;
          }
          if (e.relatedTarget && this.contains(e.relatedTarget as Node)) {
            return;
          }

          // If relatedTarget is null, the drag might have left the window or
          // an element was removed/moved during reordering. Check coordinates
          // as a fallback to verify if the pointer actually left the host.
          if (!e.relatedTarget) {
            const rect = this.getBoundingClientRect();
            const buffer = 1;
            const isOutside = e.clientX < rect.left - buffer ||
                e.clientX > rect.right + buffer ||
                e.clientY < rect.top - buffer ||
                e.clientY > rect.bottom + buffer;
            if (!isOutside) {
              return;
            }
          }

          this.isDragOverHost_ = false;
          this.reconcileKeys();
        }

        private onHostDragover_(e: DragEvent) {
          if (e.dataTransfer &&
              e.dataTransfer.types.includes(this.getMimeType())) {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
          }
        }

        private onHostDrop_(e: DragEvent) {
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
