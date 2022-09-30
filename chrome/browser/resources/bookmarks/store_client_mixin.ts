// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action, DeferredAction} from 'chrome://resources/js/store_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Store} from './store.js';
import {BookmarksPageState} from './types.js';

type Constructor<T> = new (...args: any[]) => T;

interface Watch {
  localProperty: string;
  valueGetter: (p: BookmarksPageState) => any;
}

export const StoreClientMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<StoreClientMixinInterface> => {
      class StoreClientMixin extends superClass implements
          StoreClientMixinInterface {
        private watches_: Watch[] = [];

        override connectedCallback() {
          super.connectedCallback();
          this.getStore().addObserver(this);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          this.getStore().removeObserver(this);
        }

        /**
         * Watches a particular part of the state tree, updating |localProperty|
         * to the return value of |valueGetter| whenever the state changes. Eg,
         * to keep |this.item| updated with the value of a node: watch('item',
         * (state) => state.nodes[this.itemId]);
         *
         * Note that object identity is used to determine if the value has
         * changed before updating the UI, rather than Polymer-style deep
         * equality. If the getter function returns |undefined|, no changes will
         * propagate to the UI.
         */
        private watch_(
            localProperty: string,
            valueGetter: (p: BookmarksPageState) => any): void {
          this.watches_.push({
            localProperty: localProperty,
            valueGetter: valueGetter,
          });
        }

        dispatch(action: Action|null) {
          this.getStore().dispatch(action);
        }

        dispatchAsync(action: DeferredAction) {
          this.getStore().dispatchAsync(action);
        }

        onStateChanged(newState: BookmarksPageState) {
          this.watches_.forEach((watch) => {
            const oldValue = this.get(watch.localProperty);
            const newValue = watch.valueGetter(newState);

            // Avoid poking Polymer unless something has actually changed.
            // Reducers must return new objects rather than mutating existing
            // objects, so any real changes will pass through correctly.
            if (oldValue === newValue || newValue === undefined) {
              return;
            }

            this.set(watch.localProperty, newValue);
          });
        }

        updateFromStore() {
          if (this.getStore().isInitialized()) {
            this.onStateChanged(this.getStore().data);
          }
        }

        watch(
            localProperty: string,
            valueGetter: (p: BookmarksPageState) => any) {
          this.watch_(localProperty, valueGetter);
        }

        getState() {
          return this.getStore().data;
        }

        getStore() {
          return Store.getInstance();
        }
      }
      return StoreClientMixin;
    });

export interface StoreClientMixinInterface {
  /**
   * Helper to dispatch an action to the store, which will update the store
   * data and then (possibly) flow through to the UI.
   */
  dispatch(action: Action|null): void;

  /**
   * Helper to dispatch a DeferredAction to the store, which will
   * asynchronously perform updates to the store data and UI.
   */
  dispatchAsync(action: DeferredAction): void;

  onStateChanged(newState: BookmarksPageState): void;

  updateFromStore(): void;

  watch(localProperty: string, valueGetter: (p: BookmarksPageState) => any):
      void;

  getState(): BookmarksPageState;

  getStore(): Store;
}
