// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Extended LitElement class to automatically rerender on signal
 * change.
 *
 * This is similar to SignalWatcher in lit-labs/preact-signal.
 */

import {
  LitElement,
  ReactiveController,
  ReactiveControllerHost,
} from 'chrome://resources/mwc/lit/index.js';

import {
  batch,
  Dispose,
  effect,
  ReadonlySignal,
  Signal,
  signal,
} from './signal.js';

interface PropertySignalBinding {
  signal: Signal<unknown>;
  prop: string;
}

export class ReactiveLitElement extends LitElement {
  private disposeUpdateEffect: Dispose|null = null;

  private inPerformUpdate = false;

  private readonly updateTrigger = signal<boolean>(false);

  // TODO(pihsun): Use a key to signal map if performance is an issue.
  private readonly propertySignalBindings: PropertySignalBinding[] = [];

  override performUpdate(): void {
    // We deviates with default lit semantic here and don't update while
    // disconnected. This ensures proper teardown of the update effect.
    if (!this.isUpdatePending || !this.isConnected) {
      return;
    }

    this.inPerformUpdate = true;
    if (this.disposeUpdateEffect !== null) {
      // Effect had already been set up, trigger it.
      // This lines triggers a rerun of the effect.
      this.updateTrigger.update((x) => !x);
      return;
    }

    this.disposeUpdateEffect = effect(() => {
      // Read the value of updateTrigger, so changing the value of
      // updateTrigger will triggers this effect.
      void this.updateTrigger.value;
      if (this.inPerformUpdate) {
        this.inPerformUpdate = false;
        super.performUpdate();
      } else {
        // This is being triggered by signal dependency changes in
        // super.performUpdate().
        this.requestUpdate();
      }
    });
  }

  override connectedCallback(): void {
    super.connectedCallback();
    // Request render on connect, so we know the signals used in render.
    this.requestUpdate();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.disposeUpdateEffect?.();
    this.disposeUpdateEffect = null;
  }

  registerPropertySignal(signal: Signal<unknown>, prop: string): void {
    this.propertySignalBindings.push({signal, prop});
  }

  override willUpdate(changedProperties: Map<PropertyKey, unknown>): void {
    batch(() => {
      for (const {signal, prop} of this.propertySignalBindings) {
        if (changedProperties.has(prop)) {
          // TODO(pihsun): Do we need to check changedProperties since signal
          // already does change check internally?
          signal.value = Reflect.get(this, prop);
        }
      }
    });
  }

  propSignal<T extends ReactiveLitElement, P extends string&keyof T>(
    this: T,
    prop: P,
  ): ReadonlySignal<T[P]> {
    const sig = signal(Reflect.get(this, prop));
    this.registerPropertySignal(sig, prop);
    return sig;
  }
}

export enum ComputedState {
  DONE = 'DONE',
  ERROR = 'ERROR',
  RUNNING = 'RUNNING',
  UNINITIALIZED = 'UNINITIALIZED',
}

export class ScopedAsyncComputed<T> implements ReactiveController {
  private dispose: Dispose|null = null;

  private readonly stateInternal = signal(ComputedState.UNINITIALIZED);

  // Note that this retains the latest successfully computed value even when
  // the value is being recomputing / the latest compute failed.
  // TODO(pihsun): Check if this behavior is expected.
  // TODO(pihsun): Save latest error.
  private readonly valueInternal = signal<T|null>(null);

  private readonly forceRerunToggle = signal(false);

  // Note that only the signal values before the first async point of callback
  // is tracked.
  constructor(
    host: ReactiveControllerHost,
    private readonly callback: (signal: AbortSignal) => Promise<T>,
  ) {
    host.addController(this);
  }

  rerun(): void {
    this.forceRerunToggle.update((x) => !x);
  }

  get state(): ComputedState {
    return this.stateInternal.value;
  }

  get value(): T|null {
    return this.valueInternal.value;
  }

  get valueSignal(): Signal<T|null> {
    return this.valueInternal;
  }

  hostConnected(): void {
    let latestRun!: symbol;
    let abortController: AbortController|null = null;
    this.dispose = effect(() => {
      void this.forceRerunToggle.value;

      abortController?.abort();
      abortController = new AbortController();

      const thisRun = Symbol();
      latestRun = thisRun;

      this.stateInternal.value = ComputedState.RUNNING;
      this.callback(abortController.signal)
        .then(
          (val) => {
            if (latestRun === thisRun) {
              this.valueInternal.value = val;
              this.stateInternal.value = ComputedState.DONE;
            }
          },
          (e) => {
            if (latestRun === thisRun) {
              console.error(e);
              // TODO(pihsun): Save the latest error.
              this.stateInternal.value = ComputedState.ERROR;
            }
          },
        );
    });
  }

  hostDisconnected(): void {
    this.dispose?.();
    this.dispose = null;
    this.stateInternal.value = ComputedState.UNINITIALIZED;
    this.valueInternal.value = null;
  }

  // TODO(pihsun): render() similar to @lit/task
}

// This is for exporting the class alias.
// eslint-disable-next-line @typescript-eslint/naming-convention
export const ScopedAsyncEffect = ScopedAsyncComputed<void>;
export type ScopedAsyncEffect = ScopedAsyncComputed<void>;

export class ScopedEffect implements ReactiveController {
  private dispose: Dispose|null = null;

  constructor(
    host: ReactiveControllerHost,
    private readonly callback: () => void,
  ) {
    host.addController(this);
  }

  hostConnected(): void {
    if (this.dispose === null) {
      this.dispose = effect(this.callback);
    }
  }

  hostDisconnected(): void {
    this.dispose?.();
    this.dispose = null;
  }
}
