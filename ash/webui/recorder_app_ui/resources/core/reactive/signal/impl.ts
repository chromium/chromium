// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(pihsun): Consider using the polyfill of
// https://github.com/tc39/proposal-signals or at least align our public API
// with the proposal.

import {assert} from '../../utils/assert.js';
import {IterableWeakSet} from '../../utils/iterable_weak_set.js';
import {forceCast} from '../../utils/type_utils.js';

import {Signal} from './types.js';

// All descendents of DIRTY should be DIRTY or TO_CHECK.
// All descendents of TO_CHECK should be TO_CHECK.
// Nothing should have a descendent of DISPOSED.
// TODO(pihsun): Really check what checks are needed for DISPOSED case.
enum DirtyState {
  CLEAN = 0,
  TO_CHECK = 1,
  DIRTY = 2,
  DISPOSED = 3,
}

// A parent class that holds many weak pointers to child.
interface Parent {
  // Really calculate and update value, and push dirty state down a level.
  maybeUpdate(): void;

  removeChild(child: Child): void;

  numChildrenForTesting(): void;
}

interface Child {
  addParent(parent: Parent): void;
  markDirty(): void;

  // Mark self and all children as TO_CHECK.
  markToCheckRecursive(): void;

  // Mark self dirty and recursively mark all children as TO_CHECK.
  markDirtyRecursive(): void;
}

let currentComputing: Child|null = null;

export class SignalImpl<T> extends Signal<T> implements Parent {
  private readonly children = new IterableWeakSet<Child>();

  constructor(private valueInternal: T) {
    super();
  }

  get value(): T {
    if (currentComputing !== null) {
      this.children.add(currentComputing);
      currentComputing.addParent(this);
    }
    return this.valueInternal;
  }

  set value(newValue: T) {
    if (newValue !== this.valueInternal) {
      this.valueInternal = newValue;
      // This is a micro-optimization that we never mark Signal as "dirty", but
      // instead always directly push dirty a level down, since we need to mark
      // children as TO_CHECK recursively anyway.
      for (const child of this.children) {
        child.markDirtyRecursive();
      }
      Effect.processBatchedEffect();
    }
  }

  peek(): T {
    return this.valueInternal;
  }

  maybeUpdate(): void {
    // Since we already mark children dirty when setting different value, we
    // don't need to do anything here.
    return;
  }

  removeChild(child: Child): void {
    this.children.delete(child);
  }

  numChildrenForTesting(): number {
    return this.children.sizeForTesting();
  }
}

const uninitialized = Symbol('uninitialized');

// Note that this doesn't really always implements all interface of Signal when
// `set` is not given. This is enforced at type level in the exported
// `computed` function in lib.ts.
export class ComputedImpl<T> extends Signal<T> implements Parent, Child {
  // We always start computed in dirty state, so the first value is only
  // computed when client first call to .value. Since we check for value change
  // when re-evaluating, the initial value should be something distinct to
  // possible values of the computed variable, so we can't use null/undefined
  // here and use a unique symbol as initial value instead.
  private valueInternal: T = forceCast<T>(uninitialized);

  private state = DirtyState.DIRTY;

  private readonly parents = new Set<Parent>();

  private readonly children = new IterableWeakSet<Child>();

  // Note that the `set` function should actually change the "source" value
  // depend by `get`, so the next call to `get` will get the correct value.
  constructor(
    private readonly get: () => T,
    private readonly set?: (val: T) => void,
  ) {
    super();
  }

  get value(): T {
    assert(this.state !== DirtyState.DISPOSED);

    if (currentComputing !== null) {
      this.children.add(currentComputing);
      currentComputing.addParent(this);
    }

    return this.peek();
  }

  set value(val: T) {
    assert(
      this.set !== undefined,
      'value setter called on computed without set',
    );
    this.set(val);
  }

  peek(): T {
    assert(this.state !== DirtyState.DISPOSED);

    this.maybeUpdate();

    return this.valueInternal;
  }

  markDirty(): void {
    this.state = DirtyState.DIRTY;
  }

  markDirtyRecursive(): void {
    if (this.state !== DirtyState.DIRTY) {
      this.state = DirtyState.DIRTY;
      for (const child of this.children) {
        child.markToCheckRecursive();
      }
    }
  }

  markToCheckRecursive(): void {
    if (this.state !== DirtyState.TO_CHECK && this.state !== DirtyState.DIRTY) {
      this.state = DirtyState.TO_CHECK;
      for (const child of this.children) {
        child.markToCheckRecursive();
      }
    }
  }

  addParent(parent: Parent): void {
    this.parents.add(parent);
  }

  private dispose() {
    for (const parent of this.parents) {
      parent.removeChild(this);
    }
    this.parents.clear();
    this.state = DirtyState.DISPOSED;
  }

  maybeUpdate(): void {
    if (this.state === DirtyState.CLEAN) {
      return;
    }

    if (this.state === DirtyState.TO_CHECK) {
      for (const parent of this.parents) {
        parent.maybeUpdate();
        // Typescript assumes that maybeUpdate won't change this.state, but it
        // can.
        /* eslint-disable-next-line
           @typescript-eslint/consistent-type-assertions */
        if ((this.state as DirtyState) === DirtyState.DIRTY) {
          // Someone already pass dirty state down, no need to check further.
          break;
        }
      }
      if (this.state === DirtyState.TO_CHECK) {
        // All parents clean, yay!
        this.state = DirtyState.CLEAN;
        return;
      }
    }
    // Needs update. Since the parents might change we need to dispose here.
    this.dispose();
    const oldComputing = currentComputing;
    // eslint-disable-next-line @typescript-eslint/no-this-alias
    currentComputing = this;
    const newValue = this.get();
    currentComputing = oldComputing;

    if (newValue !== this.valueInternal) {
      // Value changed, mark child as dirty.
      this.valueInternal = newValue;
      for (const child of this.children) {
        // Since this node is originally in either DIRTY or TO_CHECK, all
        // descendents should already be in DIRTY or TO_CHECK, so we don't need
        // to mark recursively here.
        child.markDirty();
      }
    }
    this.state = DirtyState.CLEAN;
  }

  removeChild(child: Child): void {
    this.children.delete(child);
  }

  numChildrenForTesting(): number {
    return this.children.sizeForTesting();
  }
}

// Set of all effects to prevent effect getting garbage collected. Effect needs
// to be cancelled explicitly with .dispose() when not in used.
const allEffects = new Set<Effect>();

export type EffectCallback = (options: {dispose: () => void}) => void;

export class Effect implements Child {
  static batchedEffect = new Set<Effect>();

  static batchDepth = 0;

  readonly parents = new Set<Parent>();

  private state = DirtyState.CLEAN;

  dispose = (): void => {
    allEffects.delete(this);
    this.state = DirtyState.DISPOSED;
    this.disconnect();
  };

  // TODO(pihsun): Have some test to ensure that there's no effect leaked.
  constructor(private readonly callback: EffectCallback) {
    // TODO(pihsun): Warning when allEffects is growing / have too many items?
    allEffects.add(this);
    this.execute();
  }

  markDirty(): void {
    if (this.state !== DirtyState.DIRTY) {
      this.state = DirtyState.DIRTY;
      Effect.batchedEffect.add(this);
    }
  }

  markDirtyRecursive(): void {
    this.markDirty();
  }

  markToCheckRecursive(): void {
    if (this.state !== DirtyState.DIRTY && this.state !== DirtyState.TO_CHECK) {
      this.state = DirtyState.TO_CHECK;
      Effect.batchedEffect.add(this);
    }
  }

  private disconnect() {
    for (const parent of this.parents) {
      parent.removeChild(this);
    }
    this.parents.clear();
  }

  private execute() {
    this.disconnect();
    const oldComputing = currentComputing;

    // eslint-disable-next-line @typescript-eslint/no-this-alias
    currentComputing = this;
    this.callback({dispose: this.dispose});
    currentComputing = oldComputing;
    // The effect might have been disposed in it's callback.
    if (this.state !== DirtyState.DISPOSED) {
      this.state = DirtyState.CLEAN;
    }
  }

  maybeExecute(): void {
    if (this.state === DirtyState.TO_CHECK) {
      for (const parent of this.parents) {
        parent.maybeUpdate();
        // Typescript assumes that maybeUpdate won't change this.state, but it
        // can.
        /* eslint-disable-next-line
           @typescript-eslint/consistent-type-assertions */
        if ((this.state as DirtyState) === DirtyState.DIRTY) {
          // Someone already pass dirty state down, no need to check further.
          break;
        }
      }
      if (this.state === DirtyState.TO_CHECK) {
        // All parents clean, yay!
        this.state = DirtyState.CLEAN;
        return;
      }
    }
    if (this.state === DirtyState.DIRTY) {
      this.execute();
    }
  }

  addParent(parent: Parent): void {
    assert(
      this.state !== DirtyState.DISPOSED,
      'addParent called after the effect had been disposed',
    );
    this.parents.add(parent);
  }

  static processBatchedEffect(): void {
    if (Effect.batchDepth !== 0) {
      return;
    }
    let cnt = 0;
    while (Effect.batchedEffect.size > 0) {
      cnt++;
      if (cnt === 10000) {
        throw new Error('Effect recurse for more than 10000 times');
      }
      const effects = Effect.batchedEffect;
      Effect.batchedEffect = new Set();
      for (const effect of effects) {
        effect.maybeExecute();
      }
    }
  }
}
