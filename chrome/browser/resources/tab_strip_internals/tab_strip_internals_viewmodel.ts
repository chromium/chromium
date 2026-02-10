// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Container} from './tab_strip_internals.mojom-webui.js';
import {TabStripInternalsApiProxyImpl} from './tab_strip_internals_api_proxy.js';
import type {TabStripInternalsApiProxy} from './tab_strip_internals_api_proxy.js';

// Observer interface implemented by Views that want to be notified when the
// ViewModel's observable state changes.
export interface ViewModelObserver {
  onViewModelChanged(): void;
}

/**
 * ViewModel layer: Handles application state and communication with backend.
 */
export class TabStripInternalsViewModel {
  private readonly proxy_: TabStripInternalsApiProxy =
      TabStripInternalsApiProxyImpl.getInstance();

  // View state
  /** Represents the root node of the navigation pane hierarchy. */
  private root_!: any;
  /** Error message exposed to the View. */
  private errorMessage_: string|null = null;
  /** Observers registered to be notified when ViewModel state changes. */
  private observers_: ViewModelObserver[] = [];

  constructor() {
    // TODO(crbug.com/427204855): Implement logic to save and restore state of
    // the view so that UI state is retained across page reloads.
  }

  get root() {
    return this.root_;
  }

  get errorMessage() {
    return this.errorMessage_;
  }

  /**
   * Entry point to initialize the ViewModel by loading data and subscribing to
   * updates.
   */
  async initialize(): Promise<void> {
    try {
      const {data} = await this.proxy_.getTabStripData();
      this.buildModelHierarchy_(data);
      this.notifyObservers_();
    } catch (e) {
      this.setError_('Failed to load TabStrip data', e);
      return;
    }

    this.proxy_.getCallbackRouter().onTabStripUpdated.addListener(
        (data: Container) => {
          try {
            this.buildModelHierarchy_(data);
            this.notifyObservers_();
          } catch (e) {
            this.setError_('Failed to apply TabStrip update', e);
          }
        });
  }

  /** View subscribes for reactive updates. */
  subscribe(observer: ViewModelObserver): void {
    this.observers_.push(observer);
  }

  /** Clears any existing error message. */
  clearError(): void {
    this.errorMessage_ = null;
  }

  /** Builds an internal representation of mojo data. */
  private buildModelHierarchy_(data: Container) {
    // TODO(crbug.com/427204855): Implement logic to adapt given mojo data into
    // a type suitable for displaying tab hierarchy on the navigation pane.
    this.root_ = data;
  }

  private setError_(msg: string, e?: unknown): void {
    console.error(msg, e);
    this.errorMessage_ = msg;
    this.notifyObservers_();
  }

  private notifyObservers_(): void {
    for (const observer of this.observers_) {
      observer.onViewModelChanged();
    }
  }
}
