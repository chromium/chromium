// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenActionName, SeaPenActions} from 'chrome://resources/ash/common/sea_pen/sea_pen_actions.js';
import {SeaPenState} from 'chrome://resources/ash/common/sea_pen/sea_pen_state.js';
import {SeaPenStoreInterface, setSeaPenStore} from 'chrome://resources/ash/common/sea_pen/sea_pen_store.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {DeferredAction, StoreObserver} from 'chrome://resources/js/store.js';

import {beginLoadSelectedImageAction, setFullscreenStateAction} from './personalization_app.js';
import {PersonalizationState} from './personalization_state.js';
import {PersonalizationStore} from './personalization_store.js';

/**
 * An adapter class that implements all of the public methods/properties of
 * SeaPenStore type. Used to delegate state and events between Personalization
 * and SeaPen.
 */
export class SeaPenStoreAdapter implements SeaPenStoreInterface,
                                           StoreObserver<PersonalizationState> {
  private observers_ = new Set<StoreObserver<SeaPenState>>();
  private priorState_: SeaPenState;

  static initSeaPenStore() {
    setSeaPenStore(new SeaPenStoreAdapter());
  }

  constructor() {
    const personalizationStore = PersonalizationStore.getInstance();
    assert(
        personalizationStore.isInitialized(),
        `${PersonalizationStore.name} must be initialized`);
    this.priorState_ = this.data;
    personalizationStore.addObserver(this);
  }

  get data(): SeaPenState {
    return PersonalizationStore.getInstance().data.wallpaper.seaPen;
  }

  init(_: SeaPenState) {
    assertNotReached(`${SeaPenStoreAdapter.name} must not be init directly`);
  }

  isInitialized(): boolean {
    return PersonalizationStore.getInstance().isInitialized();
  }

  addObserver(observer: StoreObserver<SeaPenState>) {
    this.observers_.add(observer);
  }

  removeObserver(observer: StoreObserver<SeaPenState>) {
    this.observers_.delete(observer);
  }

  hasObserver(observer: StoreObserver<SeaPenState>) {
    return this.observers_.has(observer);
  }

  beginBatchUpdate() {
    PersonalizationStore.getInstance().beginBatchUpdate();
  }

  endBatchUpdate() {
    PersonalizationStore.getInstance().endBatchUpdate();
  }

  dispatchAsync(action: DeferredAction<SeaPenActions>) {
    return PersonalizationStore.getInstance().dispatchAsync(action);
  }

  dispatch(action: SeaPenActions|null) {
    const store = PersonalizationStore.getInstance();
    this.beginBatchUpdate();
    // Dispatch additional actions to set proper wallpaper state when necessary.
    switch (action?.name) {
      // Dispatch action to set wallpaper loading state to true when Sea Pen
      // image is selected.
      case SeaPenActionName.BEGIN_SELECT_SEA_PEN_THUMBNAIL:
      case SeaPenActionName.BEGIN_LOAD_SELECTED_RECENT_SEA_PEN_IMAGE:
        store.dispatch(beginLoadSelectedImageAction());
        break;
      case SeaPenActionName.SET_SEA_PEN_FULLSCREEN_STATE:
        store.dispatch(setFullscreenStateAction(action.state));
        break;
    }
    store.dispatch(action);
    store.endBatchUpdate();
  }

  onStateChanged({wallpaper: {seaPen}}: PersonalizationState) {
    if (seaPen === this.priorState_) {
      return;
    }
    this.priorState_ = seaPen;
    for (const observer of this.observers_) {
      observer.onStateChanged(seaPen);
    }
  }
}
