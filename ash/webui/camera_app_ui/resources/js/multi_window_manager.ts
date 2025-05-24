// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * This is used to handle multiple windows of CCA.
 * An example is when using the following windows at the same time:
 * 1. CCA launched from the launcher.
 * 2. CCA launched from the ARC camera intent.
 *
 * The design principles are (The "window" below means CCA window):
 * 1. Only 0-1 windows can have the camera ownership at a time.
 * 2. A window can only start the camera after its `resumeCallback` is called.
 * 3. A window should stop the camera usage as soon as possible when the
 *    `suspendCallback` is called.
 * 4. When an active window is minimized or closed, the camera ownership is
 *    released and the ownership will be transferred to the other window if it
 *    is active or to nobody if there is no active window.
 * 5. When a window is focused, it will request the camera ownership, and the
 *    camera ownership will be transferred to the active window.
 * 6. When transferring camera ownership, always suspend the current owner, and
 *    then resume the next owner. The owner ID is changed right before resuming
 *    or entering idle state.
 * 7. If multiple windows request the camera ownership before the current owner
 *    window is suspended, only the last requester will be recorded as the next
 *    window owner.
 */

import {assert, assertNotReached} from './assert.js';
import * as comlink from './lib/comlink.js';

// This is needed since we currently have the same tsconfig for files running
// in SharedWorker and in CCA.
// TODO(b/213408699): Remove this after the tsconfig are separated.
// eslint-disable-next-line @typescript-eslint/consistent-type-assertions
const sharedWorkerScope = self as SharedWorkerGlobalScope;

interface WindowCallbacks {
  suspendCallback: () => Promise<void>;
  resumeCallback: () => Promise<void>;
}

enum OwnershipTransitionState {
  IDLE,
  SUSPENDING,
  RESUMING,
}

/**
 * Run in a shared worker to handle multi-window use case.
 */
class MultiWindowManager {
  // Map from window ID to the corresponding callbacks.
  private readonly windowCallbacksMap = new Map<number, WindowCallbacks>();

  private windowCounter = 0;

  /**
   * The ID of the window owning the camera usage.
   * Since the ID will be used as the target window when doing suspend/resume,
   * the transition of the camera ownership happens before trying to resume the
   * window.
   */
  private currentOwnerId: number|null = null;

  /**
   * The pending transition, null if no pending transition.
   * If non-null, contains the ID of the window which the camera ownership will
   * be transferred to after suspension, or {id: null} if the current ownership
   * should be suspended.
   * We only keep one ID so if multiple windows request camera usage while
   * suspending, only the last one will be recorded as the next owner.
   */
  private pendingTransition: {id: number|null}|null = null;

  /**
   * The state to indicate the transition of the camera ownership. When
   * transition cycle starts, it will switch between SUSPENDING/RESUMING until
   * the next owner is empty. Once the transition is finished, the state should
   * go back to IDLE.
   *
   * All the suspend/resume callbacks are chained on the calls when the
   * `transitionState` is IDLE, and all calls should only change the
   * `pendingTransition` when `transitionState` is not IDLE.
   */
  private transitionState: OwnershipTransitionState =
      OwnershipTransitionState.IDLE;

  createWindowInstance() {
    const newWindow = new WindowInstanceImpl(this.windowCounter);
    this.windowCounter++;
    return newWindow;
  }

  async registerWindow(id: number, windowCallbacks: WindowCallbacks):
      Promise<void> {
    this.windowCallbacksMap.set(id, windowCallbacks);
    await this.notifyWindowRestored(id);
  }

  async notifyWindowClosed(id: number): Promise<void> {
    this.windowCallbacksMap.delete(id);
    await this.notifyWindowMinimized(id);
  }

  async notifyWindowRestored(id: number): Promise<void> {
    switch (this.transitionState) {
      case OwnershipTransitionState.IDLE:
        assert(this.pendingTransition === null);
        if (this.currentOwnerId === null) {
          this.currentOwnerId = id;
          await this.resumeWindow();
        } else if (this.currentOwnerId !== id) {
          this.pendingTransition = {id};
          await this.suspendWindow();
        }
        break;
      case OwnershipTransitionState.SUSPENDING:
        this.pendingTransition = {id};
        break;
      case OwnershipTransitionState.RESUMING:
        if (this.currentOwnerId === id) {
          this.pendingTransition = null;
        } else {
          this.pendingTransition = {id};
        }
        break;
      default:
        assertNotReached(
            `Unexpected transition state: ${this.transitionState}`);
    }
  }

  async notifyWindowMinimized(id: number) {
    if (id === this.pendingTransition?.id) {
      // We don't return here since if a window is activated and then minimized
      // while it is suspending, this.pendingTransition.id ===
      // this.currentOwnerId, and we still need to handle the transition.
      this.pendingTransition = null;
    }

    if (id !== this.currentOwnerId) {
      return;
    }

    switch (this.transitionState) {
      case OwnershipTransitionState.IDLE:
        await this.suspendWindow();
        break;
      case OwnershipTransitionState.SUSPENDING:
        break;
      case OwnershipTransitionState.RESUMING:
        this.pendingTransition = {id: null};
        break;
      default:
        assertNotReached(
            `Unexpected transition state: ${this.transitionState}`);
    }
  }

  getActiveWindowCallbacks(): WindowCallbacks|null {
    if (this.currentOwnerId === null) {
      return null;
    }
    return this.windowCallbacksMap.get(this.currentOwnerId) ?? null;
  }

  async suspendWindow(): Promise<void> {
    const callbacks = this.getActiveWindowCallbacks();
    if (callbacks === null) {
      return this.resumeNextOrIdle();
    }
    this.transitionState = OwnershipTransitionState.SUSPENDING;
    await callbacks.suspendCallback();
    await this.resumeNextOrIdle();
  }

  async resumeWindow(): Promise<void> {
    const callbacks = this.getActiveWindowCallbacks();
    if (callbacks === null) {
      return this.resumeNextOrIdle();
    }
    this.transitionState = OwnershipTransitionState.RESUMING;
    await callbacks.resumeCallback();
    if (this.pendingTransition !== null) {
      await this.suspendWindow();
    } else {
      this.transitionState = OwnershipTransitionState.IDLE;
    }
  }

  async resumeNextOrIdle(): Promise<void> {
    this.currentOwnerId = this.pendingTransition?.id ?? null;
    this.pendingTransition = null;

    if (this.currentOwnerId !== null) {
      await this.resumeWindow();
    } else {
      this.transitionState = OwnershipTransitionState.IDLE;
    }
  }
}

class WindowInstanceImpl {
  constructor(private readonly id: number) {}

  async init(
      suspendCallback: () => Promise<void>,
      resumeCallback: () => Promise<void>): Promise<void> {
    await windowManager.registerWindow(
        this.id, {suspendCallback, resumeCallback});
  }

  async onVisibilityChanged(isVisible: boolean): Promise<void> {
    if (isVisible) {
      await windowManager.notifyWindowRestored(this.id);
    } else {
      await windowManager.notifyWindowMinimized(this.id);
    }
  }

  async onWindowClosed(): Promise<void> {
    await windowManager.notifyWindowClosed(this.id);
  }
}

const windowManager = new MultiWindowManager();

// Only export types to ensure that the file is not imported by other files at
// runtime.
export type WindowInstance = WindowInstanceImpl;

/**
 * Triggers when the Shared Worker is connected.
 */
sharedWorkerScope.onconnect = (event: MessageEvent) => {
  const port = event.ports[0];
  comlink.expose(windowManager.createWindowInstance(), port);
  port.start();
};
