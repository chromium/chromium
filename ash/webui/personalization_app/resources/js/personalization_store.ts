// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/ash/common/cr_elements/list_property_update_mixin.js';
import {makeStoreClientMixin} from 'chrome://resources/ash/common/cr_elements/store_client/store_client.js';
import {Store} from 'chrome://resources/js/store.js';

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

const PersonalizationStoreClientMixin =
    makeStoreClientMixin(PersonalizationStore.getInstance);

/**
 * A base class for Personalization App polymer elements to access useful
 * utilities like i18n and store client methods.
 */
export const WithPersonalizationStore = I18nMixin(
    ListPropertyUpdateMixin(PersonalizationStoreClientMixin(PolymerElement)));
