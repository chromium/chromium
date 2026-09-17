// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import {OneShotTimer} from '../../timer.js';
import type {WebClientHost} from '../request_types.js';
import type {                     //
             InterfaceDef,        //
             PendingRemote,       //
             PostMessageHandler,  //
             PostMessageReceiver, //
             PostMessageRemote,   //
             PostMessageRouter,   //
} from '../transport/post_message_transport.js';

export interface ObservableSetByTabIdDelegate<
    ObservedType, ObserverInterface extends InterfaceDef = InterfaceDef> {
  readonly interfaceDef: ObserverInterface;
  readonly unsubscribeDelay: number;

  subscribe(
      clientRemote: PostMessageRemote<WebClientHost>, tabId: string,
      remote: PendingRemote<ObserverInterface>): void;
  createHandler(observable: ObservableValueImpl<ObservedType>):
      PostMessageHandler<ObserverInterface>;
}

// Manages a set of observables which each observe a tab.
// When a tab is closed, the corresponding observable is completed, and
// removed from the set. Otherwise, observables are kept in the set,
// so they can be re-subscribed to later.
export class ObservableSetByTabId<
    ObservedType, ObserverInterface extends InterfaceDef = InterfaceDef> {
  private observablesByTabId = new Map<
      string,
      ObservableSetByTabIdObservable<ObservedType, ObserverInterface>>();

  constructor(
      private delegate:
          ObservableSetByTabIdDelegate<ObservedType, ObserverInterface>,
      private clientRemote: PostMessageRemote<WebClientHost>,
      private router: PostMessageRouter) {}

  getObservableByTabId(tabId: string):
      ObservableSetByTabIdObservable<ObservedType, ObserverInterface> {
    let obs = this.observablesByTabId.get(tabId);
    if (obs !== undefined) {
      return obs;
    }
    obs = new ObservableSetByTabIdObservable<ObservedType, ObserverInterface>(
        tabId, this.clientRemote, this.router, this.delegate, () => {
          this.observablesByTabId.delete(tabId);
        });
    this.observablesByTabId.set(tabId, obs);
    return obs;
  }
}

// An observable representing a lazy, reference-counted, and debounced
// stream of updates for a specific tab from the host.
//
// It connects when the first subscriber joins, disconnects with a delay when
// the last subscriber leaves, and cleans itself up on completion.
export class ObservableSetByTabIdObservable<
    ObservedType, ObserverInterface extends InterfaceDef = InterfaceDef> extends
    ObservableValueImpl<ObservedType> {
  private unsubscribeTimer: OneShotTimer;
  private receiver?: PostMessageReceiver;
  private isCompleting = false;

  constructor(
      public tabId: string,
      private clientRemote: PostMessageRemote<WebClientHost>,
      private router: PostMessageRouter,
      private delegate:
          ObservableSetByTabIdDelegate<ObservedType, ObserverInterface>,
      private onComplete: () => void) {
    super(/*isSet=*/ false);
    this.unsubscribeTimer = new OneShotTimer(delegate.unsubscribeDelay);
  }

  override activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    if (!hasActiveSubscription) {
      this.unsubscribeTimer.start(() => {
        if (this.hasActiveSubscription()) {
          return;
        }
        this.complete();
      });
      return;
    }
    this.unsubscribeTimer.reset();
    if (!this.receiver) {
      const {receiver, remote} =
          this.router.newPipeWithReceiver<ObserverInterface>(
              this.delegate.createHandler(this), this.delegate.interfaceDef);
      this.receiver = receiver;
      this.receiver.addCloseHandler(() => {
        this.complete();
      });
      this.delegate.subscribe(this.clientRemote, this.tabId, remote);
    }
  }

  override complete() {
    // As this is an observable, it can be completed only once. Early exit if
    // already complete.
    if (this.isCompleting || this.isStopped()) {
      return;
    }
    this.isCompleting = true;
    this.receiver?.close();
    this.receiver = undefined;
    this.onComplete();
    super.complete();
  }
}
