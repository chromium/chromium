// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for handling dragging elements in a container.
 *     Draggable elements must have the 'draggable' attribute set.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';
import {Constructor} from '../common/types.js';

export interface Position {
  x: number;
  y: number;
}

/**
 * Type of an ongoing drag.
 */
enum DragType {
  NONE = 0,
  CURSOR = 1,
  KEYBOARD = 2,
}

type DragCallback = (id: string, amount: Position|null) => void;

export interface DragMixinInterface {
  dragId: string;
  dragEnabled: boolean;
  keyboardDragEnabled: boolean;
  keyboardDragStepSize: number;
  initializeDrag(
      enabled: boolean, container?: HTMLElement, callback?: DragCallback): void;
}

export const DragMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(baseClass: T): T&
    Constructor<DragMixinInterface> => {
      class DragMixinInternal extends baseClass implements DragMixinInterface {
        static get properties() {
          return {
            /** Whether or not drag is enabled (e.g. not mirrored). */
            dragEnabled: Boolean,

            /**
             * Whether or not to allow keyboard dragging.  If set to false,
             * all keystrokes will be ignored by this element.
             */
            keyboardDragEnabled: {
              type: Boolean,
              value: false,
            },

            /**
             * The number of pixels to drag on each keypress.
             */
            keyboardDragStepSize: {
              type: Number,
              value: 20,
            },
          };
        }

        dragEnabled: boolean;
        /**
         * The id of the element being dragged, or empty if not dragging.
         */
        dragId: string = '';
        keyboardDragEnabled: boolean;
        keyboardDragStepSize: number;

        /**
         * The type of the currently ongoing drag.  If a keyboard drag is
         * ongoing and the user initiates a cursor drag, the keyboard drag
         * should end before the cursor drag starts.  If a cursor drag is
         * onging, keyboard dragging should be ignored.
         */
        private dragType_: DragType = DragType.NONE;
        private dragOffset_: Position;
        private container_: HTMLElement|undefined;
        private callback_: DragCallback|null;
        private dragStartLocation_: Position = {x: 0, y: 0};
        /**
         * Used to ignore unnecessary drag events.
         */
        private lastTouchLocation_: Position|null = null;
        private mouseDownListener_ = this.onMouseDown_.bind(this);
        private mouseMoveListener_ = this.onMouseMove_.bind(this);
        private touchStartListener_ = this.onTouchStart_.bind(this);
        private touchMoveListener_ = this.onTouchMove_.bind(this);
        private keyDownListener_ = this.onKeyDown_.bind(this);
        private endDragListener_ = this.endCursorDrag_.bind(this);

        initializeDrag(
            enabled: boolean, container?: HTMLElement,
            callback?: DragCallback): void {
          this.dragEnabled = enabled;
          if (!enabled) {
            this.removeListeners_();
            return;
          }

          if (container) {
            this.container_ = container;
          }
          if (callback) {
            this.callback_ = callback;
          }

          this.addListeners_();
        }

        private addListeners_(): void {
          const container = this.container_;
          if (!container) {
            return;
          }

          container.addEventListener('mousedown', this.mouseDownListener_);
          container.addEventListener('mousemove', this.mouseMoveListener_);
          container.addEventListener('touchstart', this.touchStartListener_);
          container.addEventListener('touchmove', this.touchMoveListener_);
          container.addEventListener('keydown', this.keyDownListener_);
          container.addEventListener('touchend', this.endDragListener_);
          window.addEventListener('mouseup', this.endDragListener_);
        }

        private removeListeners_(): void {
          const container = this.container_;
          if (!container || !this.mouseDownListener_) {
            return;
          }

          container.removeEventListener('mousedown', this.mouseDownListener_);
          container.removeEventListener('mousemove', this.mouseMoveListener_);
          container.removeEventListener('touchstart', this.touchStartListener_);
          container.removeEventListener('touchmove', this.touchMoveListener_);
          container.removeEventListener('keydown', this.keyDownListener_);
          container.removeEventListener('touchend', this.endDragListener_);
          window.removeEventListener('mouseup', this.endDragListener_);
        }

        private onMouseDown_(e: MouseEvent): boolean {
          const target = cast(e.target, HTMLElement);
          if (e.button !== 0 || !target.getAttribute('draggable')) {
            return true;
          }
          e.preventDefault();
          return this.startCursorDrag_(target, {x: e.pageX, y: e.pageY});
        }

        private onMouseMove_(e: MouseEvent): boolean {
          e.preventDefault();
          return this.processCursorDrag_({x: e.pageX, y: e.pageY});
        }

        private onTouchStart_(e: TouchEvent): boolean {
          if (e.touches.length !== 1) {
            return false;
          }

          e.preventDefault();
          const target = cast(e.target, HTMLElement);
          const touch = e.touches[0];
          this.lastTouchLocation_ = {x: touch.pageX, y: touch.pageY};
          return this.startCursorDrag_(target, this.lastTouchLocation_);
        }

        private onTouchMove_(e: TouchEvent): boolean {
          if (e.touches.length !== 1) {
            return true;
          }

          const touchLocation = {x: e.touches[0].pageX, y: e.touches[0].pageY};
          // Touch move events can happen even if the touch location doesn't
          // change and on small unintentional finger movements. Ignore these
          // small changes.
          if (this.lastTouchLocation_) {
            const IGNORABLE_TOUCH_MOVE_PX = 1;
            const xDiff = Math.abs(touchLocation.x - this.lastTouchLocation_.x);
            const yDiff = Math.abs(touchLocation.y - this.lastTouchLocation_.y);
            if (xDiff <= IGNORABLE_TOUCH_MOVE_PX &&
                yDiff <= IGNORABLE_TOUCH_MOVE_PX) {
              return true;
            }
          }
          this.lastTouchLocation_ = touchLocation;
          e.preventDefault();
          return this.processCursorDrag_(touchLocation);
        }

        private onKeyDown_(e: KeyboardEvent): boolean {
          // Ignore keystrokes if keyboard dragging is disabled.
          if (this.keyboardDragEnabled === false) {
            return true;
          }

          // Ignore keystrokes if the event target is not draggable.
          const target = cast(e.target, HTMLElement);
          if (!target.getAttribute('draggable')) {
            return true;
          }

          // Keyboard drags should not interrupt cursor drags.
          if (this.dragType_ === DragType.CURSOR) {
            return true;
          }

          let delta: Position;
          switch (e.key) {
            case 'ArrowUp':
              delta = {x: 0, y: -this.keyboardDragStepSize};
              break;
            case 'ArrowDown':
              delta = {x: 0, y: this.keyboardDragStepSize};
              break;
            case 'ArrowLeft':
              delta = {x: -this.keyboardDragStepSize, y: 0};
              break;
            case 'ArrowRight':
              delta = {x: this.keyboardDragStepSize, y: 0};
              break;
            case 'Enter':
              e.preventDefault();
              this.endKeyboardDrag_();
              return false;
            default:
              return true;
          }

          e.preventDefault();

          if (this.dragType_ === DragType.NONE) {
            // Start drag
            this.startKeyboardDrag_(target);
          }

          this.dragOffset_.x += delta.x;
          this.dragOffset_.y += delta.y;

          this.processKeyboardDrag_(this.dragOffset_);

          return false;
        }

        private startCursorDrag_(target: HTMLElement, eventLocation: Position):
            boolean {
          assert(this.dragEnabled);
          if (this.dragType_ === DragType.KEYBOARD) {
            this.endKeyboardDrag_();
          }
          this.dragId = target.id;
          this.dragStartLocation_ = eventLocation;
          this.dragType_ = DragType.CURSOR;
          return false;
        }

        private endCursorDrag_(): boolean {
          assert(this.dragEnabled);
          if (this.dragType_ === DragType.CURSOR && this.callback_) {
            this.callback_(this.dragId, null);
          }
          this.cleanupDrag_();
          return false;
        }

        private processCursorDrag_(eventLocation: Position): boolean {
          assert(this.dragEnabled);
          if (this.dragType_ !== DragType.CURSOR) {
            return true;
          }
          this.executeCallback_(eventLocation);
          return false;
        }

        private startKeyboardDrag_(target: HTMLElement): void {
          assert(this.dragEnabled);
          if (this.dragType_ === DragType.CURSOR) {
            this.endCursorDrag_();
          }
          this.dragId = target.id;
          this.dragStartLocation_ = {x: 0, y: 0};
          this.dragOffset_ = {x: 0, y: 0};
          this.dragType_ = DragType.KEYBOARD;
        }

        private endKeyboardDrag_(): void {
          assert(this.dragEnabled);
          if (this.dragType_ === DragType.KEYBOARD && this.callback_) {
            this.callback_(this.dragId, null);
          }
          this.cleanupDrag_();
        }

        private processKeyboardDrag_(dragPosition: Position): boolean {
          assert(this.dragEnabled);
          if (this.dragType_ !== DragType.KEYBOARD) {
            return true;
          }
          this.executeCallback_(dragPosition);
          return false;
        }

        /**
         * Cleans up state for all currently ongoing drags.
         */
        private cleanupDrag_(): void {
          this.dragId = '';
          this.dragStartLocation_ = {x: 0, y: 0};
          this.lastTouchLocation_ = null;
          this.dragType_ = DragType.NONE;
        }

        private executeCallback_(dragPosition: Position): void {
          if (this.callback_) {
            const delta = {
              x: dragPosition.x - this.dragStartLocation_.x,
              y: dragPosition.y - this.dragStartLocation_.y,
            };
            this.callback_(this.dragId, delta);
          }
        }
      }

      return DragMixinInternal;
    });
