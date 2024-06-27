// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An iterable version of WeakSet.
 *
 * TODO(pihsun): Unit tests.
 */
export class IterableWeakSet<T extends object> {
  private readonly set = new Set<WeakRef<T>>();

  private readonly weakRefMap = new WeakMap<T, WeakRef<T>>();

  private readonly finalizationGroup = new FinalizationRegistry(
    (ref: WeakRef<T>) => {
      this.set.delete(ref);
    },
  );

  add(it: T): void {
    if (this.weakRefMap.has(it)) {
      return;
    }
    const weakRef = new WeakRef(it);
    this.weakRefMap.set(it, weakRef);
    this.set.add(weakRef);
    this.finalizationGroup.register(it, weakRef, weakRef);
  }

  delete(it: T): void {
    const weakRef = this.weakRefMap.get(it);
    if (weakRef !== undefined) {
      this.weakRefMap.delete(it);
      this.set.delete(weakRef);
      this.finalizationGroup.unregister(weakRef);
    }
  }

  /**
   * Gets the estimated size.
   *
   * Note that this is just an estimate, and relies on garbage collection on
   * whether the finalization has been run or not, so this should only be used
   * in testing.
   */
  sizeForTesting(): number {
    return this.set.size;
  }

  * [Symbol.iterator](): Iterator<T> {
    for (const ref of this.set) {
      const it = ref.deref();
      if (it !== undefined) {
        yield it;
      }
    }
  }

  has(it: T): boolean {
    return this.weakRefMap.get(it)?.deref() !== undefined;
  }
}
