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

  protected constructor(
      private isSet: boolean, private value?: T,
      private hasActiveSubscriptionCallback?:
          (hasActiveSubscription: boolean) => void) {}

  /**
   * Create an ObservableValue which has an initial value. Optionally a
   * `hasActiveSubscriptionCallback` can be added which will be called with
   * `true` when this observable has its first subscriber and `false` when there
   * are no longer any subscribers to this observable.
   */
  static withValue<T>(
      value: T,
      hasActiveSubscriptionCallback?:
          (hasActiveSubscription: boolean) => void) {
    return new ObservableValue(true, value, hasActiveSubscriptionCallback);
  }

  /**
   * Create an ObservableValue which has no initial value. Subscribers will not
   * be called until after assignAndSignal() is called the first time.
   * Optionally a `hasActiveSubscriptionCallback` can be added which will be
   * called with `true` when this observable has its first subscriber and
   * false` when there are no longer any subscribers to this observable.
   */
  static withNoValue<T>(
      hasActiveSubscriptionCallback?:
          (hasActiveSubscription: boolean) => void) {
    return new ObservableValue<T>(
        false, undefined, hasActiveSubscriptionCallback);
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

  onUnsubscribe(sub: ObservableSubscription<T>) {
    if (!this.subscribers) {
      return;
    }
    if (this.subscribers.size === 0) {
      return;
    }

    this.subscribers.delete(sub);
    if (this.subscribers.size === 0) {
      this.activeSubscriptionChanged(false);
    }
  }

  /** Receive updates for value changes. */
  subscribe(change: (newValue: T) => void): Subscriber {
    const newSub =
        new ObservableSubscription(change, this.onUnsubscribe.bind(this));
    if (this.subscribers.size === 0) {
      this.activeSubscriptionChanged(true);
    }
    this.subscribers.add(newSub);
    if (this.isSet) {
      change(this.value!);
    }
    return newSub;
  }

  protected activeSubscriptionChanged(hasActiveSubscription: boolean): void {
    this.hasActiveSubscriptionCallback?.(hasActiveSubscription);
  }

  protected hasActiveSubscription(): boolean {
    return this.subscribers.size > 0;
  }

  /**
   * Asynchronously waits until the ObservableValue's current value satisfies a
   * given criteria.
   */
  async waitUntil(criteria: (value: T) => boolean): Promise<T> {
    const {promise, resolve} = Promise.withResolvers<T>();
    const sub = this.subscribe((newValue) => {
      if (criteria(newValue)) {
        resolve(newValue);
      }
    });
    const resultValue = await promise;
    sub.unsubscribe();
    return resultValue;
  }
}

/**
 * A simple observable with no memory of previous values.
 */
export class Subject<T> {
  private subscribers: Set<ObservableSubscription<T>> = new Set();

  subscribe(change: (newValue: T) => void): Subscriber {
    const newSub =
        new ObservableSubscription(change, this.onUnsubscribe.bind(this));
    this.subscribers.add(newSub);
    return newSub;
  }

  next(v: T) {
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

  private onUnsubscribe(sub: ObservableSubscription<T>) {
    this.subscribers.delete(sub);
  }
}
