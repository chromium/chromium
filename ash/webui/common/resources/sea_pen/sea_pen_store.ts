// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/ash/common/cr_elements/list_property_update_mixin.js';
import {makeStoreClientMixin} from 'chrome://resources/ash/common/cr_elements/store_client/store_client.js';
import {Store} from 'chrome://resources/js/store.js';

import {SeaPenActions} from './sea_pen_actions.js';
import {seaPenReducer} from './sea_pen_reducer.js';
import {emptyState, SeaPenState} from './sea_pen_state.js';

export class SeaPenStore extends Store<SeaPenState, SeaPenActions> {
  constructor() {
    super(emptyState(), seaPenReducer);
  }
}

// A helper type to extract public properties from a concrete class
// implementation.
type Public<T> = {
  [K in keyof T]: T[K]
};

export type SeaPenStoreInterface = Public<Store<SeaPenState, SeaPenActions>>;

let instance: SeaPenStoreInterface|null = null;

export function setSeaPenStore(store: SeaPenStoreInterface) {
  instance = store;
}

export function getSeaPenStore(): SeaPenStoreInterface {
  return instance || (instance = new SeaPenStore());
}

// SeaPenStoreInterface implements all public methods/properties of
// SeaPenStore, but concrete class types as used in makeStoreClientMixin also
// check private properties. This cast bypasses this.
const SeaPenStoreClientMixin = makeStoreClientMixin(
    getSeaPenStore as () => Store<SeaPenState, SeaPenActions>);

export const WithSeaPenStore =
    I18nMixin(ListPropertyUpdateMixin(SeaPenStoreClientMixin(PolymerElement)));
