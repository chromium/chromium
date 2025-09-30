// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';

import type {TabsEvent, TabsObserverInterface, TabsObserverPendingReceiverEndpoint} from './tab_strip_api.mojom-webui.js';
import {TabsEventFieldTags, TabsObserverReceiver, whichTabsEvent} from './tab_strip_api.mojom-webui.js';
import type {TabStripObserver} from './tab_strip_observer.js';

/**
 * @fileoverview
 * This file defines the TabStripObservation, a TypeScript client for the
 * TabsObserver mojom interface. The TabStripObservation is used in conjunction
 * with the TabStripObserver.
 *
 * The TabStripObservation handles the underlying message pipe and the message
 * dispatch, while the TabStripObserver provides an interface for the clients
 * to receive the messages.
 *
 * ...
 *
 * @example
 * // Get the TabStripService remote and create a new router.
 * const service = TabStripService.getRemote();
 * const myObserver = new MyGreatTabStripObserver();
 * const observation = new TabStripObservation(myObserver);
 *
 * // Fetch the initial tab state and the observer stream handle.
 * const snapshot = await service.getTabs();
 * // Messages might immediately dispatch on this call, if there are queued
 * // up messages.
 * observationRouter.bind(snapshot.stream.handle);
 *
 */
export class TabStripObservation implements TabsObserverInterface {
  private readonly observer_: TabStripObserver;
  private readonly receiver_: TabsObserverReceiver;

  constructor(observer: TabStripObserver) {
    this.observer_ = observer;
    this.receiver_ = new TabsObserverReceiver(this);
  }

  // If there are events already queued, this call will immediately dispatch
  // the messages to the observer associated with this observation.
  bind(handle: TabsObserverPendingReceiverEndpoint) {
    // TODO(crbug.com/439639253): throw error if already bound. This will
    // already throw, but the msg is probably not very helpful.
    this.receiver_.$.bindHandle(handle);
  }

  private notify_(event: TabsEvent) {
    const which = whichTabsEvent(event);
    switch (which) {
      case TabsEventFieldTags.DATA_CHANGED_EVENT:
        this.observer_.onDataChanged(event.dataChangedEvent!);
        break;
      case TabsEventFieldTags.COLLECTION_CREATED_EVENT:
        this.observer_.onCollectionCreated(event.collectionCreatedEvent!);
        break;
      case TabsEventFieldTags.NODE_MOVED_EVENT:
        this.observer_.onNodeMoved(event.nodeMovedEvent!);
        break;
      case TabsEventFieldTags.TABS_CLOSED_EVENT:
        this.observer_.onTabsClosed(event.tabsClosedEvent!);
        break;
      case TabsEventFieldTags.TABS_CREATED_EVENT:
        this.observer_.onTabsCreated(event.tabsCreatedEvent!);
        break;
      default:
        assertNotReachedCase(which);
    }
  }

  onTabEvents(events: TabsEvent[]) {
    for (const event of events) {
      this.notify_(event);
    }
  }
}
