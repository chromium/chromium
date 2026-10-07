// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import {OneShotTimer} from '../../timer.js';

// Manages a set of observables which each observe a tab.
// When a tab is closed, the corresponding observable is completed, and
// removed from the set. Otherwise, observables are kept in the set,
// so they can be re-subscribed to later.
export class TabObservableSet<T, O extends TabObservable<T>> {
  private observablesByTabId = new Map<string, O>();

  constructor(private factory: (tabId: string, onComplete: () => void) => O) {}

  getObservableByTabId(tabId: string): O {
    let obs = this.observablesByTabId.get(tabId);
    if (obs !== undefined) {
      return obs;
    }
    obs = this.factory(tabId, () => {
      this.observablesByTabId.delete(tabId);
    });
    this.observablesByTabId.set(tabId, obs);
    return obs;
  }
}

// An observable representing a lazy, reference-counted, and debounced
// stream of updates for a specific tab.
//
// It connects when the first subscriber joins, disconnects with a delay when
// the last subscriber leaves, and cleans itself up on completion.
export abstract class TabObservable<T> extends ObservableValueImpl<T> {
  private unsubscribeTimer: OneShotTimer;
  private isCompleting = false;

  constructor(
      public readonly tabId: string, protected readonly onComplete: () => void,
      unsubscribeDelay = 1000) {
    super(/*isSet=*/ false);
    this.unsubscribeTimer = new OneShotTimer(unsubscribeDelay);
  }

  protected override activeSubscriptionChanged(hasActiveSubscription: boolean):
      void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (this.isCompleting || this.isStopped()) {
      return;
    }
    if (!hasActiveSubscription) {
      this.unsubscribeTimer.start(() => {
        if (this.hasActiveSubscription()) {
          return;
        }
        this.disconnectFromSource();
      });
      return;
    }
    this.unsubscribeTimer.reset();
    if (!this.isConnected()) {
      this.connectToSource();
    }
  }

  protected abstract isConnected(): boolean;
  protected abstract connectToSource(): void;
  protected abstract disconnectFromSource(): void;

  override complete() {
    // As this is an observable, it can be completed only once. Early exit if
    // already complete.
    if (this.isCompleting || this.isStopped()) {
      return;
    }
    this.isCompleting = true;
    this.unsubscribeTimer.reset();
    this.disconnectFromSource();
    this.onComplete();
    super.complete();
  }
}
