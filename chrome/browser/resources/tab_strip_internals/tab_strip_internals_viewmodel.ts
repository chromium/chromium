// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Container} from './tab_strip_internals.mojom-webui.js';
import {TabStripInternalsApiProxyImpl} from './tab_strip_internals_api_proxy.js';
import type {TabStripInternalsApiProxy} from './tab_strip_internals_api_proxy.js';

// Observer interface implemented by Views that want to be notified when the
// ViewModel's observable state changes.
export interface ViewModelObserver {
  onViewModelChanged(change: ViewModelChange): void;
}

/** Represents types of changes in the ViewModel's observable state. */
export enum ViewModelChange {
  /** Represents presentational change in layout. */
  LAYOUT,
  /** Represents a notification. */
  NOTIFICATION,
  /** Represents a change in view content. */
  CONTENT,
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
  /** Navigation pane constraints used to save/load state. */
  private static readonly NAV_PANE_MIN_WIDTH_PX: number = 200;
  private static readonly NAV_PANE_MAX_WIDTH_PX: number = 800;
  private static readonly NAV_PANE_DEFAULT_WIDTH_PX: number = 320;
  private navPaneWidth_ = TabStripInternalsViewModel.NAV_PANE_DEFAULT_WIDTH_PX;

  constructor() {
    this.loadState_();
  }

  get root() {
    return this.root_;
  }

  get errorMessage() {
    return this.errorMessage_;
  }

  get navPaneWidth() {
    return this.navPaneWidth_;
  }

  /**
   * Entry point to initialize the ViewModel by loading data and subscribing to
   * updates.
   */
  async initialize(): Promise<void> {
    try {
      const {data} = await this.proxy_.getTabStripData();
      this.buildModelHierarchy_(data);
    } catch (e) {
      this.setError_('Failed to load TabStrip data', e);
      return;
    }

    this.proxy_.getCallbackRouter().onTabStripUpdated.addListener(
        (data: Container) => {
          try {
            this.buildModelHierarchy_(data);
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

  /** Set width of the navigation pane. */
  setNavPaneWidth(px: number) {
    const clamped = Math.min(
        Math.max(TabStripInternalsViewModel.NAV_PANE_MIN_WIDTH_PX, px),
        TabStripInternalsViewModel.NAV_PANE_MAX_WIDTH_PX);
    if (clamped === this.navPaneWidth_) {
      return;
    }
    this.navPaneWidth_ = clamped;
    this.notifyObservers_(ViewModelChange.LAYOUT);
  }

  /** Persists UI state to localStorage. */
  saveState() {
    // TODO(crbug.com/427204855): Implement logic to save and restore state of
    // the navigation pane (selected and expanded state).
    localStorage.setItem(
        'tabstrip_internals_state',
        JSON.stringify({
          navPaneWidth: this.navPaneWidth_,
        }),
    );
  }

  /** Builds an internal representation of mojo data. */
  private buildModelHierarchy_(data: Container) {
    // TODO(crbug.com/427204855): Implement logic to adapt given mojo data into
    // a type suitable for displaying tab hierarchy on the navigation pane.
    this.root_ = data;
    this.notifyObservers_(ViewModelChange.CONTENT);
  }

  /** Loads UI state from localStorage. */
  private loadState_() {
    // TODO(crbug.com/427204855): Implement logic to save and restore state of
    // the navigation pane (selected and expanded state).
    try {
      const state =
          JSON.parse(localStorage.getItem('tabstrip_internals_state') || '{}');
      this.navPaneWidth_ = typeof state.navPaneWidth === 'number' ?
          state.navPaneWidth :
          TabStripInternalsViewModel.NAV_PANE_DEFAULT_WIDTH_PX;
    } catch (e) {
      console.warn(
          'Failed to load TabStripInternals state from localStorage', e);
    }
  }

  private setError_(msg: string, e?: unknown): void {
    console.error(msg, e);
    this.errorMessage_ = msg;
    this.notifyObservers_(ViewModelChange.NOTIFICATION);
  }

  private notifyObservers_(change: ViewModelChange): void {
    for (const observer of this.observers_) {
      observer.onViewModelChanged(change);
    }
  }
}
