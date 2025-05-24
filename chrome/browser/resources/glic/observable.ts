// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this code is made to conform to glic_api's Observable, but can also be
// used independently.

/** Allows control of a subscription to an ObservableValue. */
export declare interface Subscriber {
  unsubscribe(): void;
}

class ObservableSubscription<T> implements Subscriber {
  constructor(
      public onChange: (newValue: T) => void,
      private onUnsubscribe: (self: ObservableSubscription<T>) => void) {}

  unsubscribe(): void {
    this.onUnsubscribe(this);
  }
}

export interface ObservableValueReadOnly<T> {
  getCurrentValue(): T|undefined;
  subscribe(change: (newValue: T) => void): Subscriber;
}

/**
 * A observable value that can change over time. If value is initialized, sends
 * it to new subscribers upon subscribe().
 */
export class ObservableValue<T> {
  private subscribers: Set<ObservableSubscription<T>> = new Set();

  private constructor(private isSet: boolean, private value: T|undefined) {}

  /** Create an ObservableValue which has an initial value. */
  static withValue<T>(value: T) {
    return new ObservableValue(true, value);
  }

  /**
   * Create an ObservableValue which has no initial value. Subscribers will not
   * be called until after assignAndSignal() is called the first time.
   */
  static withNoValue<T>() {
    return new ObservableValue<T>(false, undefined);
  }

  assignAndSignal(v: T, force = false) {
    const send = !this.isSet || this.value !== v || force;
    this.isSet = true;
    this.value = v;
    if (!send) {
      return;
    }
    this.subscribers.forEach((sub) => {
      // Ignore if removed since forEach was called.
      if (this.subscribers.has(sub)) {
        try {
          sub.onChange(v);
        } catch (e) {
          console.warn(e);
        }
      }
    });
  }

  /** Returns the current value, or undefined if not initialized. */
  getCurrentValue(): T|undefined {
    return this.value;
  }

  /** Receive updates for value changes. */
  subscribe(change: (newValue: T) => void): Subscriber {
    const newSub = new ObservableSubscription(
        change, (sub) => this.subscribers.delete(sub));
    this.subscribers.add(newSub);
    if (this.isSet) {
      change(this.value!);
    }
    return newSub;
  }
}
