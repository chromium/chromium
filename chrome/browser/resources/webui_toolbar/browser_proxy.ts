// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/js/cr.js';

import {BrowserControlsService} from '/shared/browser_controls_api.mojom-webui.js';
import type {BrowserControlsServiceInterface} from '/shared/browser_controls_api.mojom-webui.js';
import type {IconUpdate} from '/shared/icon_handle.mojom-webui.js';
import {ToolbarUIObserverCallbackRouter, ToolbarUIService} from '/shared/toolbar_ui_api.mojom-webui.js';
import type {ToolbarUIServiceInterface, ToolbarUIServiceRemote} from '/shared/toolbar_ui_api.mojom-webui.js';
import type {FocusRequestTarget, ToolbarState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

export type ToolbarStateListener = (icons: IconUpdate[], state: ToolbarState) =>
    void;

export type ToolbarStateListenerHandle = number;
export const INVALID_TOOLBAR_STATE_LISTENER_HANDLE: ToolbarStateListenerHandle =
    -1;
/*
 * Listener type for invocations to
 *    toolbar_ui_api.mojom.ToolbarUIObserver.OnFocusRequested method.
 *
 * Declare handle:
 *   listenerHandle : FocusRequestHandle = INVALID_FOCUS_REQUEST_HANDLE;
 *
 * Subscribe:
 *   listenerHandle = browserProxy.addFocusRequestListener(
 *       (target: FocusRequestTarget) => {
 *   });
 *
 * Unsubscribe:
 *   browserProxy.removeFocusRequestListener(listenerHandle);
 *
 * Calling removeFocus with INVALID_FOCUS_REQUEST_HANDLE is OK; so it's
 * the preferred initialization value for FocusRequestHandle type.
 */
export type FocusRequestListener = (target: FocusRequestTarget) => void;
export type FocusRequestHandle = number;
export const INVALID_FOCUS_REQUEST_HANDLE: FocusRequestHandle = -1;

export type ShowSplitTabsContextMenuListener = () => void;
export type ShowSplitTabsContextMenuHandle = number;
export const INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE:
    ShowSplitTabsContextMenuHandle = -1;

import type {PermissionChipDelegate} from '/shared/permission_chip_delegate.js';
import type {LhsChipIdentifier} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

/**
 * Keys corresponding to initial toolbar state values provided by the browser.
 */
export enum ToolbarStateKey {
  IS_NAVIGATION_LOADING = 'isNavigationLoading',
  RELOAD_CAN_SHOW_MENU = 'reloadCanShowMenu',
  BACK_BUTTON_ENABLED = 'backButtonEnabled',
  FORWARD_BUTTON_ENABLED = 'forwardButtonEnabled',
  HOME_BUTTON_SHOULD_BE_SHOWN = 'homeButtonShouldBeShown',
  BATTERY_SAVER_BUTTON_VISIBLE = 'batterySaverButtonVisible',
  LAYOUT_CONSTANTS_VERSION = 'layoutConstantsVersion',
  TOUCH_UI = 'touchUi',
  INITIAL_WEBUI_SURFACE_SYNC_ENABLED = 'initialWebUISurfaceSyncEnabled',
  IS_FALLBACK_PREWARMING = 'isFallbackPrewarming',
  IS_PROFILE_ELIGIBLE_FOR_GLIC = 'isProfileEligibleForGlic',
  CAN_SHOW_AVATAR_FOR_PROFILE = 'canShowAvatarForProfile',
}

/**
 * Schema mapping `ToolbarStateKey` entries to their respective value types in
 * the initial state dictionary.
 */
export interface ToolbarFlatStateSchema {
  [ToolbarStateKey.IS_NAVIGATION_LOADING]: boolean;
  [ToolbarStateKey.RELOAD_CAN_SHOW_MENU]: boolean;
  [ToolbarStateKey.BACK_BUTTON_ENABLED]: boolean;
  [ToolbarStateKey.FORWARD_BUTTON_ENABLED]: boolean;
  [ToolbarStateKey.HOME_BUTTON_SHOULD_BE_SHOWN]: boolean;
  [ToolbarStateKey.BATTERY_SAVER_BUTTON_VISIBLE]: boolean;
  [ToolbarStateKey.LAYOUT_CONSTANTS_VERSION]: number;
  [ToolbarStateKey.TOUCH_UI]: boolean;
  [ToolbarStateKey.INITIAL_WEBUI_SURFACE_SYNC_ENABLED]: boolean;
  [ToolbarStateKey.IS_FALLBACK_PREWARMING]: boolean;
  [ToolbarStateKey.IS_PROFILE_ELIGIBLE_FOR_GLIC]: boolean;
  [ToolbarStateKey.CAN_SHOW_AVATAR_FOR_PROFILE]: boolean;
}

/**
 * Wraps the parsed initial toolbar state dictionary and provides typed getters.
 */
export class InitialStateReader {
  private state_: Record<string, unknown>;

  constructor(state: Record<string, unknown> = {}) {
    this.state_ = state;
  }

  static fromJson(jsonString: string): InitialStateReader {
    if (jsonString) {
      try {
        const parsed = JSON.parse(jsonString);
        if (parsed && typeof parsed === 'object') {
          return new InitialStateReader(parsed as Record<string, unknown>);
        }
      } catch {
        // Fall through to empty state.
      }
    }
    return new InitialStateReader();
  }

  /**
   * Determines whether the specified `key` is present in the initial toolbar
   * state dictionary.
   */
  hasKey(key: ToolbarStateKey): boolean {
    return key in this.state_;
  }

  /**
   * Reads a boolean property for the given `key` from the initial state
   * dictionary. Returns `false` if the property is missing or not a boolean.
   */
  getBoolean<K extends keyof ToolbarFlatStateSchema>(key: K): boolean {
    const val = this.state_[key];
    return typeof val === 'boolean' ? val : false;
  }

  /**
   * Reads a numeric property for the given `key` from the initial state
   * dictionary. Returns `0` if the property is missing or not a number.
   */
  getInteger<K extends keyof ToolbarFlatStateSchema>(key: K): number {
    const val = this.state_[key];
    return typeof val === 'number' ? val : 0;
  }
}

export interface BrowserProxy extends PermissionChipDelegate {
  browserControlsHandler: BrowserControlsServiceInterface;
  toolbarUIHandler: ToolbarUIServiceInterface;

  /**
   * Returns the parsed and cached initial toolbar state reader.
   */
  getInitialState(): InitialStateReader;

  /**
   * Records a value in a histogram.
   * @param histogramName The name of the histogram.
   * @param value The value to record.
   * @param maxValue The maximum value of the histogram.
   */
  recordInHistogram(histogramName: string, value: number, maxValue: number):
      void;

  addToolbarStateListener(listener: ToolbarStateListener):
      ToolbarStateListenerHandle;
  addFocusRequestListener(listener: FocusRequestListener): FocusRequestHandle;
  addShowSplitTabsContextMenuListener(
      listener: ShowSplitTabsContextMenuListener):
      ShowSplitTabsContextMenuHandle;

  removeToolbarStateListener(handle: ToolbarStateListenerHandle): void;
  removeFocusRequestListener(handle: FocusRequestHandle): void;
  removeShowSplitTabsContextMenuListener(
      handle: ShowSplitTabsContextMenuHandle): void;

  /**
   * Resolves once the browser has dispatched every message sent so far on the
   * ToolbarUIService pipe (e.g. onOmniboxAction). For tests that need the
   * browser-side state to reflect what this page has already sent.
   */
  flushToolbarUiHandlerForTesting(): Promise<void>;
}

export class BrowserProxyImpl implements BrowserProxy {
  private callbackRouter: ToolbarUIObserverCallbackRouter;
  private toolbarUIRemote: ToolbarUIServiceRemote;
  private initialState_: InitialStateReader|null = null;
  browserControlsHandler: BrowserControlsServiceInterface;
  toolbarUIHandler: ToolbarUIServiceInterface;

  private constructor() {
    this.callbackRouter = new ToolbarUIObserverCallbackRouter();
    this.browserControlsHandler = BrowserControlsService.getRemote();
    this.toolbarUIRemote = ToolbarUIService.getRemote();
    this.toolbarUIHandler = this.toolbarUIRemote;
  }

  onChipClicked(id: LhsChipIdentifier, isPointer: boolean, stateToken: number) {
    this.toolbarUIHandler.onLhsChipClicked(id, isPointer, stateToken);
  }

  onChipPointerEntered(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipPointerEntered(id);
  }

  onChipPointerExited(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipPointerExited(id);
  }

  onChipMousePressed(id: LhsChipIdentifier, isMiddleClick: boolean = false) {
    this.toolbarUIHandler.onLhsChipMousePressed(id, isMiddleClick);
  }

  onChipExpandAnimationEnded(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipExpandAnimationEnded(id);
  }

  onChipCollapseAnimationEnded(id: LhsChipIdentifier) {
    this.toolbarUIHandler.onLhsChipCollapseAnimationEnded(id);
  }

  getInitialState(): InitialStateReader {
    if (!this.initialState_) {
      this.initialState_ =
          InitialStateReader.fromJson(chrome.getVariableValue('initialState'));
    }
    return this.initialState_;
  }

  /**
   * Records a value in a histogram.
   * @param histogramName The name of the histogram.
   * @param value The value to record.
   * @param maxValue The maximum value of the histogram.
   */
  recordInHistogram(histogramName: string, value: number, maxValue: number) {
    chrome.send(
        'metricsHandler:recordInHistogram', [histogramName, value, maxValue]);
  }

  addToolbarStateListener(listener: ToolbarStateListener) {
    const handle =
        this.callbackRouter.onToolbarStateChanged.addListener(listener);
    this.toolbarUIHandler.bind().then(fence => {
      listener(fence.icons, fence.state);
      this.callbackRouter.$.bindHandle(fence.updateStream.handle);
    });
    return handle;
  }

  addFocusRequestListener(listener: FocusRequestListener) {
    // This assumes addToolbarStateListener will happen or has happened to
    // actually connect the router.
    return this.callbackRouter.onFocusRequested.addListener(listener);
  }

  addShowSplitTabsContextMenuListener(
      listener: ShowSplitTabsContextMenuListener) {
    return this.callbackRouter.showSplitTabsContextMenu.addListener(listener);
  }

  removeToolbarStateListener(handle: ToolbarStateListenerHandle) {
    if (handle !== INVALID_TOOLBAR_STATE_LISTENER_HANDLE) {
      this.callbackRouter.removeListener(handle);
    }
  }

  removeFocusRequestListener(handle: FocusRequestHandle) {
    if (handle !== INVALID_FOCUS_REQUEST_HANDLE) {
      this.callbackRouter.removeListener(handle);
    }
  }

  removeShowSplitTabsContextMenuListener(
      handle: ShowSplitTabsContextMenuHandle) {
    if (handle !== INVALID_SHOW_SPLIT_TABS_CONTEXT_MENU_HANDLE) {
      this.callbackRouter.removeListener(handle);
    }
  }

  flushToolbarUiHandlerForTesting(): Promise<void> {
    return this.toolbarUIRemote.$.flushForTesting();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(proxy: BrowserProxy) {
    instance = proxy;
  }
}

let instance: BrowserProxy|null = null;
