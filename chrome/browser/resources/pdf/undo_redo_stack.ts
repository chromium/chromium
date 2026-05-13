// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {TextAnnotation} from './constants.js';

export interface TextUndoRedoState {
  type: 'text';
  before: TextAnnotation|null;
  after: TextAnnotation|null;
}

export type UndoRedoState = {
  type: 'ink',
}|TextUndoRedoState;

export interface UndoRedoStateChangedDetail {
  canUndo: boolean;
  canRedo: boolean;
  hasUnsavedEdits: boolean;
}

export class UndoRedoStack {
  private stack_: UndoRedoState[] = [];
  private pointer_: number = -1;
  private savedPointer_: number = -1;
  private savingPointer_: number = -1;
  private eventTarget_: EventTarget;

  constructor(eventTarget: EventTarget) {
    this.eventTarget_ = eventTarget;
  }

  private dispatchStateChanged_() {
    const detail: UndoRedoStateChangedDetail = {
      canUndo: this.canUndo(),
      canRedo: this.canRedo(),
      hasUnsavedEdits: this.isDirty(),
    };
    this.eventTarget_.dispatchEvent(
        new CustomEvent('undo-redo-state-changed', {detail}));
  }

  push(state: UndoRedoState) {
    // Remove everything past the current pointer, and push new change.
    this.stack_.splice(this.pointer_ + 1);
    this.stack_.push(state);
    // Last saved state that can no longer be returned to.
    if (this.savedPointer_ > this.pointer_) {
      this.savedPointer_ = -1;  // Invalidate
    }
    if (this.savingPointer_ > this.pointer_) {
      // Mark as invalid so whenever save is completed, the save pointer
      // will be invalidated also.
      this.savingPointer_ = -1;
    }
    this.pointer_++;
    this.dispatchStateChanged_();
  }

  undo(): UndoRedoState|null {
    if (!this.canUndo()) {
      return null;
    }
    const state = this.stack_[this.pointer_]!;
    this.pointer_--;
    this.dispatchStateChanged_();
    return state;
  }

  redo(): UndoRedoState|null {
    if (!this.canRedo()) {
      return null;
    }
    this.pointer_++;
    const state = this.stack_[this.pointer_]!;
    this.dispatchStateChanged_();
    return state;
  }

  canUndo(): boolean {
    return this.pointer_ >= 0;
  }

  canRedo(): boolean {
    return this.pointer_ < this.stack_.length - 1;
  }

  initiateSave() {
    assert(this.savingPointer_ === -1);
    this.savingPointer_ = this.pointer_;
  }

  setSaved() {
    const changed = this.savedPointer_ !== this.savingPointer_;
    this.savedPointer_ = this.savingPointer_;
    this.savingPointer_ = -1;
    if (changed) {
      this.dispatchStateChanged_();
    }
  }

  cancelSave() {
    this.savingPointer_ = -1;
  }

  isDirty(): boolean {
    return this.pointer_ !== this.savedPointer_;
  }

  resetForTesting() {
    this.stack_ = [];
    this.pointer_ = -1;
    this.savedPointer_ = -1;
    this.savingPointer_ = -1;
    this.dispatchStateChanged_();
  }
}
