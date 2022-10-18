// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action, Store} from 'chrome://resources/ash/common/store/store.js';
import {StoreClient, StoreClientInterface} from 'chrome://resources/ash/common/store/store_client.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {IronResizableBehavior} from 'chrome://resources/polymer/v3_0/iron-resizable-behavior/iron-resizable-behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {reduce} from './personalization_reducers.js';
import {emptyState, PersonalizationState} from './personalization_state.js';

/**
 * @fileoverview A singleton datastore for the personalization app. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store. Any data that is shared between components should go here.
 */

export class PersonalizationStore extends Store<PersonalizationState> {
  constructor() {
    // `reduce` has a more specific type, need to relax it for parent
    // constructor.
    super(
        emptyState(),
        reduce as (state: PersonalizationState, action: Action) =>
            PersonalizationState);
  }

  static getInstance(): PersonalizationStore {
    return instance || (instance = new PersonalizationStore());
  }

  static setInstance(newInstance: PersonalizationStore): void {
    instance = newInstance;
  }
}

let instance: PersonalizationStore|null = null;

export interface PersonalizationStoreClient {
  watch<T>(
      localProperty: string,
      valueGetter: (state: PersonalizationState) => T): void;

  getState(): PersonalizationState;

  getStore(): PersonalizationStore;
}

// These map to internals of store.js and store_client.js and are a weird
// workaround from pre-typescript days. Closure compiler didn't do a great job
// with generics, so there were public methods that were typed that called into
// the following untyped private methods. Other apps use their own existing
// workarounds for this, so to avoid breaking them by rewriting store and
// store_client, add our own type declarations here.
interface PropertyWatch {
  localProperty: string;
  valueGetter<T>(state: PersonalizationState): T;
}

interface PersonalizationStoreClientPrivate {
  onStateChanged(
      this: PolymerElement&PersonalizationStoreClientPrivate,
      newState: PersonalizationState): void;
  watch_: typeof PersonalizationStoreClientImpl['watch'];
  watches_: PropertyWatch[];
}

const PersonalizationStoreClientImpl: PersonalizationStoreClient&
    Pick<PersonalizationStoreClientPrivate, 'onStateChanged'> = {
      /**
       * Override onStateChanged so property updates are batched. Reduces churn
       * in polymer components when multiple state values change.
       */
      onStateChanged(
          this: PolymerElement&PersonalizationStoreClient&
          PersonalizationStoreClientPrivate,
          newState: PersonalizationState): void {
        const changes = this.watches_.reduce(
            (result: Record<string, any>, watch: PropertyWatch) => {
              const oldValue = this.get(watch.localProperty);
              const newValue = watch.valueGetter(newState);
              if (newValue !== oldValue && newValue !== undefined) {
                result[watch.localProperty] = newValue;
              }
              return result;
            },
            {});
        this.setProperties(changes);
      },

      watch<T>(
          this: PersonalizationStoreClientPrivate, localProperty: string,
          valueGetter: (state: PersonalizationState) => T) {
        this.watch_<T>(localProperty, valueGetter);
      },

      getState(): PersonalizationState {
        return this.getStore().data;
      },

      getStore(): PersonalizationStore {
        return PersonalizationStore.getInstance();
      },
    };

export const WithPersonalizationStore =
    mixinBehaviors(
        [
          StoreClient,
          PersonalizationStoreClientImpl,
          IronResizableBehavior,
        ],
        I18nMixin(ListPropertyUpdateMixin(PolymerElement))) as {
      new (): PolymerElement & I18nMixinInterface & IronResizableBehavior &
          ListPropertyUpdateMixinInterface & PersonalizationStoreClient &
          StoreClientInterface<PersonalizationState>,
    };
