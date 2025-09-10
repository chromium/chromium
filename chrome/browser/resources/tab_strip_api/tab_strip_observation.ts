// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';

import type {TabsEvent, TabsObserverInterface} from './tab_strip_api.mojom-webui.js';
import {TabsEventFieldTags, TabsObserverReceiver, whichTabsEvent} from './tab_strip_api.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnDataChangedEvent, OnTabMovedEvent, OnTabsClosedEvent, OnTabsCreatedEvent} from './tab_strip_api_events.mojom-webui.js';

type CallbackType<EventType> = (event: EventType) => void;

class Channel<EventType> {
  private listeners_: Array<CallbackType<EventType>> = [];

  // TODO(crbug.com/439639253): add removeListener

  addListener(listener: CallbackType<EventType>) {
    this.listeners_.push(listener);
  }

  notify(event: EventType): void {
    for (const listener of this.listeners_) {
      listener(event);
    }
  }
}

/**
 * @fileoverview
 * This file defines the TabStripObservation, a TypeScript client for the
 * TabsObserver mojom interface.
 *
 * ...
 *
 * @example
 * // Get the TabStripService remote and create a new router.
 * const service = TabStripService.getRemote();
 * const observation = new TabStripObservation();
 *
 * // Fetch the initial tab state and the observer stream handle.
 * const snapshot = await service.getTabs();
 * observationRouter.bind((snapshot.stream as any).handle);
 *
 * // Add listeners for the events you want to handle.
 * observationRouter.onTabsCreated.addListener((event) => {
 *   // Logic to add new tabs to the UI.
 *   for (const tabContainer of event.tabs) {
 *     myUi.addTab(tabContainer.tab);
 *   }
 * });
 *
 */
export class TabStripObservation implements TabsObserverInterface {
  readonly onDataChanged = new Channel<OnDataChangedEvent>();
  readonly onCollectionCreated = new Channel<OnCollectionCreatedEvent>();
  readonly onTabMoved = new Channel<OnTabMovedEvent>();
  readonly onTabsClosed = new Channel<OnTabsClosedEvent>();
  readonly onTabsCreated = new Channel<OnTabsCreatedEvent>();

  private readonly receiver_: TabsObserverReceiver;

  constructor() {
    this.receiver_ = new TabsObserverReceiver(this);
  }

  bind(handle: any) {
    // TODO(crbug.com/439639253): throw error if already bound. This will
    // already throw, but the msg is probably not very helpful.
    this.receiver_.$.bindHandle(handle);
  }

  onTabEvents(events: TabsEvent[]) {
    for (const event of events) {
      this.notify_(event);
    }
  }

  private notify_(event: TabsEvent) {
    const which = whichTabsEvent(event);
    switch (which) {
      case TabsEventFieldTags.DATA_CHANGED_EVENT:
        this.onDataChanged.notify(event.dataChangedEvent!);
        break;
      case TabsEventFieldTags.COLLECTION_CREATED_EVENT:
        this.onCollectionCreated.notify(event.collectionCreatedEvent!);
        break;
      case TabsEventFieldTags.TAB_MOVED_EVENT:
        this.onTabMoved.notify(event.tabMovedEvent!);
        break;
      case TabsEventFieldTags.TABS_CLOSED_EVENT:
        this.onTabsClosed.notify(event.tabsClosedEvent!);
        break;
      case TabsEventFieldTags.TABS_CREATED_EVENT:
        this.onTabsCreated.notify(event.tabsCreatedEvent!);
        break;
      default:
        assertNotReachedCase(which);
    }
  }
}
