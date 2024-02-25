// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action, DeferredAction, Store} from '//resources/js/store.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview defines a helper function `makeStoreClientMixin` to create a
 * Polymer mixin that binds Polymer elements to a specific instance of `Store`.
 * The mixin provides utility functions for Polymer elements to dispatch actions
 * that change state, and to react to Store state changes.
 */

/**
 * A callback function that runs when the store state has been updated.
 * Returning `undefined` will skip updating the local property.
 * @see StoreClientInterface.watch
 */
export interface ValueGetter<S, V> {
  (state: S): V|undefined;
}

export interface StoreClientInterface<S, A extends Action> {
  /**
   * Helper to dispatch an action to the store, which will update the store data
   * and then (possibly) flow through to the UI.
   */
  dispatch(action: A|null): void;

  /**
   * Helper to dispatch an asynchronous action to the store.
   * TODO(b/296440261) remove `dispatchAsync` in favor of promises.
   */
  dispatchAsync(action: DeferredAction<A>): void;

  // Called when the store state has changed.
  onStateChanged(state: S): void;

  /**
   * Call this when the element is connected and has called `watch` for its
   * properties. This will populate the element with the initial
   * data from the store if the store has been initialized.
   */
  updateFromStore(): void;

  /**
   * Watches a particular part of the state tree, updating `localProperty` to
   * the return value of `valueGetter` whenever the state changes.
   *
   * Note that object identity is used to determine if the value has changed
   * before updating, rather than deep equality. If the getter function
   * returns `undefined`, no changes will be propagated.
   */
  watch<V>(localProperty: string, valueGetter: ValueGetter<S, V>): void;

  // Get the current state from the store.
  getState(): S;

  // Get the store that this client is bound to.
  getStore(): Store<S, A>;
}

type Constructor<T> = new (...args: any[]) => T;

/**
 * Create a store client mixin for the store instance returned by
 * `storeGetter()`. An app, such as Personalization App, will have one central
 * store to bind to. Example:
 *
 * class MyStore extends Store {
 *   static getInstance(): MyStore {
 *     ....
 *   }
 * }
 *
 * const MyStoreClientMixin = makeStoreClientMixin(MyStore.getInstance);
 *
 * const MyElement extends MyStoreClientMixin(PolymerElement) {
 *   ....
 * }
 */
export function makeStoreClientMixin<S, A extends Action>(
    storeGetter: () => Store<S, A>) {
  function storeClientMixin<T extends Constructor<PolymerElement>>(
      superClass: T): T&Constructor<StoreClientInterface<S, A>> {
    class StoreClientMixin extends superClass implements
        StoreClientInterface<S, A> {
      private propertyWatches_: Map<string, ValueGetter<S, any>> = new Map();

      override connectedCallback() {
        super.connectedCallback();
        this.getStore().addObserver(this);
      }

      override disconnectedCallback() {
        super.disconnectedCallback();
        this.getStore().removeObserver(this);
      }

      dispatch(action: A): void {
        this.getStore().dispatch(action);
      }

      dispatchAsync(action: DeferredAction<A>): void {
        this.getStore().dispatchAsync(action);
      }

      onStateChanged(state: S) {
        // Collect all changes and batch them together. This reduces visual
        // churn on the polymer component if a single store change results in
        // multiple polymer properties changing.
        const changes: Record<string, any> = {};
        for (const [localProperty, valueGetter] of this.propertyWatches_) {
          const oldValue = this.get(localProperty);
          const newValue = valueGetter(state);
          if (newValue !== oldValue && newValue !== undefined) {
            changes[localProperty] = newValue;
          }
        }
        this.setProperties(changes);
      }

      updateFromStore(): void {
        // TODO(b/296282541) assert that store is initialized instead of
        // performing a runtime check.
        if (this.getStore().isInitialized()) {
          this.onStateChanged(this.getStore().data);
        }
      }

      watch<V>(localProperty: string, valueGetter: ValueGetter<S, V>) {
        if (this.propertyWatches_.has(localProperty)) {
          console.warn(`Overwriting watch for property ${localProperty}`);
        }
        this.propertyWatches_.set(localProperty, valueGetter);
      }

      getState(): S {
        return this.getStore().data;
      }

      getStore(): Store<S, A> {
        return storeGetter();
      }
    }

    return StoreClientMixin;
  }

  return dedupingMixin(storeClientMixin);
}
