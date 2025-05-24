// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Action, DeferredAction, StoreObserver} from '//resources/js/store.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {Store} from './store.js';
import type {BookmarksPageState} from './types.js';

type Constructor<T> = new (...args: any[]) => T;

export const StoreClientMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<StoreClientMixinLitInterface> => {
      class StoreClientMixinLit extends superClass implements
          StoreClientMixinLitInterface {
        override connectedCallback() {
          super.connectedCallback();
          Store.getInstance().addObserver(this);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          Store.getInstance().removeObserver(this);
        }

        dispatch(action: Action): void {
          Store.getInstance().dispatch(action);
        }

        dispatchAsync(action: DeferredAction<Action>): void {
          Store.getInstance().dispatchAsync(action);
        }

        updateFromStore(): void {
          // TODO(b/296282541) assert that store is initialized instead of
          // performing a runtime check.
          if (Store.getInstance().isInitialized()) {
            this.onStateChanged(this.getState());
          }
        }

        onStateChanged(_state: BookmarksPageState) {
          // Should be overridden by clients who want to modify private state
          // in response to state changes.
        }

        getState(): BookmarksPageState {
          return Store.getInstance().data;
        }

        getStore(): Store {
          return Store.getInstance();
        }
      }
      return StoreClientMixinLit;
    };

export interface StoreClientMixinLitInterface extends
    StoreObserver<BookmarksPageState> {
  /**
   * Helper to dispatch an action to the store, which will update the store data
   * and then (possibly) flow through to the UI.
   */
  dispatch(action: Action|null): void;

  /**
   * Helper to dispatch an asynchronous action to the store.
   * TODO(b/296440261) remove `dispatchAsync` in favor of promises.
   */
  dispatchAsync(action: DeferredAction<Action>): void;

  /**
   * Call this when the element is connected and has called `watch` for its
   * properties. This will populate the element with the initial
   * data from the store if the store has been initialized.
   */
  updateFromStore(): void;

  // Get the current state from the store.
  getState(): BookmarksPageState;

  // Get the store that this client is bound to.
  getStore(): Store;
}
