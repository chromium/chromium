// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenImageId} from './constants.js';
import {SeaPenObserverInterface, SeaPenObserverReceiver, SeaPenProviderInterface, TextQueryHistoryEntry} from './sea_pen.mojom-webui.js';
import {setSeaPenTextQueryHistory, setSelectedRecentSeaPenImageAction} from './sea_pen_actions.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {getSeaPenStore} from './sea_pen_store.js';

let instance: SeaPenObserver|null = null;

/**
 * Set up the observer to listen for SeaPen changes.
 */
function initSeaPenObserver(
    seaPenProvider: SeaPenProviderInterface,
    target: SeaPenObserverInterface): SeaPenObserverReceiver {
  const receiver = new SeaPenObserverReceiver(target);
  seaPenProvider.setSeaPenObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}

export class SeaPenObserver implements SeaPenObserverInterface {
  static initSeaPenObserverIfNeeded(): void {
    if (!instance) {
      instance = new SeaPenObserver();
    }
  }

  private receiver_: SeaPenObserverReceiver =
      initSeaPenObserver(getSeaPenProvider(), this);

  onSelectedSeaPenImageChanged(id: SeaPenImageId|null): void {
    const store = getSeaPenStore();
    store.dispatch(setSelectedRecentSeaPenImageAction(id));
  }

  onTextQueryHistoryChanged(entries: TextQueryHistoryEntry[]|null): void {
    const store = getSeaPenStore();
    store.dispatch(setSeaPenTextQueryHistory(entries));
  }
}
