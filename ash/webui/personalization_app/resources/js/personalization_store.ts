// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {Store} from 'chrome://resources/js/store_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Actions} from './personalization_actions.js';
import {reduce} from './personalization_reducers.js';
import {emptyState, PersonalizationState} from './personalization_state.js';

/**
 * @fileoverview A singleton datastore for the personalization app. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store. Any data that is shared between components should go here.
 */


/**
 * The singleton instance of PersonalizationStore. Constructed once when the
 * app starts. Replaced in tests.
 */
let instance: PersonalizationStore|null = null;

/**
 * A Personalization App specific version of a Store with singleton getter and
 * setter.
 */
export class PersonalizationStore extends Store<PersonalizationState, Actions> {
  constructor() {
    super(emptyState(), reduce);
  }

  static getInstance(): PersonalizationStore {
    return instance || (instance = new PersonalizationStore());
  }

  static setInstance(newInstance: PersonalizationStore): void {
    instance = newInstance;
  }
}

/**
 * A bridge between Polymer elements and PersonalizationStore. Allows elements
 * to respond to user interaction and dispatch events, and also observe state
 * changes.
 */
export interface PersonalizationStoreClientMixinInterface {
  dispatch(action: Actions): void;

  onStateChanged(state: PersonalizationState): void;

  /**
   * Call when a polymer element is ready to initialize it with data from the
   * store.
   */
  updateFromStore(): void;

  /**
   * Observe the value returned by `valueGetter` and assign it to
   * `this[localProperty]` when it changes.
   */
  watch<T>(
      localProperty: string,
      valueGetter: (state: PersonalizationState) => T): void;

  /** Get the current state in the store. */
  getState(): PersonalizationState;

  /** Get the currently registered PersonalizationStore singleton. */
  getStore(): Store<PersonalizationState, Actions>;
}

type Constructor<T> = new (...args: any[]) => T;

interface PropertyWatch {
  localProperty: string;
  valueGetter: (state: PersonalizationState) => any;
}

/** Polymer element mixin to allow elements to access store client methods. */
const PersonalizationStoreClientMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(elementClass: T):
        Constructor<PolymerElement>&
    Constructor<PersonalizationStoreClientMixinInterface> => {
      class PersonalizationStoreClientImpl extends elementClass implements
          PersonalizationStoreClientMixinInterface {
        private watches_: PropertyWatch[] = [];

        override connectedCallback() {
          super.connectedCallback();
          this.getStore().addObserver(this);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          this.getStore().removeObserver(this);
        }

        dispatch(action: Actions|null) {
          this.getStore().dispatch(action);
        }

        onStateChanged(state: PersonalizationState) {
          const changes = this.watches_.reduce(
              (result: Record<string, any>, {localProperty, valueGetter}) => {
                const oldValue = this.get(localProperty);
                const newValue = valueGetter(state);
                if (newValue !== oldValue && newValue !== undefined) {
                  result[localProperty] = newValue;
                }
                return result;
              },
              {});
          this.setProperties(changes);
        }

        updateFromStore() {
          if (this.getStore().isInitialized()) {
            this.onStateChanged(this.getStore().data);
          }
        }

        watch<T>(
            localProperty: string,
            valueGetter: (state: PersonalizationState) => T) {
          this.watches_.push({localProperty, valueGetter});
        }

        getState() {
          return this.getStore().data;
        }

        getStore(): PersonalizationStore {
          return PersonalizationStore.getInstance();
        }
      }

      return PersonalizationStoreClientImpl;
    });

/**
 * A base class for Personalization App polymer elements to access useful
 * utilities like i18n and store client methods.
 */
export const WithPersonalizationStore = I18nMixin(
    ListPropertyUpdateMixin(PersonalizationStoreClientMixin(PolymerElement)));
