// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this code is made to conform to glic_api's Observable, but can also be
// used independently.

/** Allows control of a subscription to an ObservableValue. */
export declare interface Subscriber {
  unsubscribe(): void;
}

/** Observes an Observable. */
export declare interface Observer<T> {
  /** Called when the Observable emits a value. */
  next?(value: T): void;
  /**
   * Called if the Observable emits an error. If an error is emitted, no
   * additional updates will be sent.
   */
  error?(err: any): void;
  /**
   * Called when the Observable completes. If complete is called, no additional
   * updates will be sent.
   */
  complete?(): void;
}

export interface ObservableValueReadOnly<T> {
  getCurrentValue(): T|undefined;
  subscribe(change: (newValue: T) => void): Subscriber;
  subscribeObserver(observer: Observer<T>): Subscriber;
}

class ObservableSubscription<T> implements Subscriber {
  constructor(
      public observer: Observer<T>,
      private onUnsubscribe: (self: ObservableSubscription<T>) => void) {}

  unsubscribe(): void {
    this.onUnsubscribe(this);
  }
}

/**
 * A base class for observables that handles subscribers.
 */
class ObservableBase<T> {
  private subscribers: Set<ObservableSubscription<T>> = new Set();
  private state_: 'active'|'complete'|'error' = 'active';
  private errorValue?: Error;

  protected next(value: T): void {
    switch (this.state_) {
      case 'active':
        break;
      case 'complete':
      case 'error':
        throw new Error('Observable is not active');
    }
    this.subscribers.forEach((sub) => {
      // Ignore if removed since forEach was called.
      if (this.subscribers.has(sub)) {
        try {
          sub.observer.next?.(value);
        } catch (e) {
          console.warn(e);
        }
      }
    });
  }

  error(e: Error): void {
    switch (this.state_) {
      case 'active':
        this.state_ = 'error';
        this.errorValue = e;
        break;
      case 'complete':
      case 'error':
        throw new Error('Observable is not active');
    }
    let loggedWarning = false;
    const hadSubscribers = this.hasActiveSubscription();
    this.subscribers.forEach((sub) => {
      // Ignore if removed since forEach was called.
      if (this.subscribers.has(sub)) {
        if (sub.observer.error) {
          try {
            sub.observer.error(e);
          } catch (e) {
            console.warn(e);
          }
        } else {
          if (!loggedWarning) {
            console.warn('unhandled error: ', e);
            loggedWarning = true;
          }
        }
      }
    });
    this.subscribers.clear();
    if (hadSubscribers){
      this.activeSubscriptionChanged(false);
    }
  }

  complete(): void {
    switch (this.state_) {
      case 'active':
        this.state_ = 'complete';
        break;
      case 'complete':
      case 'error':
        throw new Error('Observable is not active');
    }
    const hadSubscribers = this.hasActiveSubscription();
    this.subscribers.forEach((sub) => {
      // Ignore if removed since forEach was called.
      if (this.subscribers.has(sub)) {
        try {
          sub.observer.complete?.();
        } catch (e) {
          console.warn(e);
        }
      }
    });
    this.subscribers.clear();
    if (hadSubscribers){
      this.activeSubscriptionChanged(false);
    }
  }

  isStopped(): boolean {
    return this.state_ !== 'active';
  }

  /** Subscribe to changes. */
  subscribe(change: (newValue: T) => void): Subscriber;
  subscribe(observer: Observer<T>): Subscriber;
  subscribe(changeOrObserver: ((newValue: T) => void)|Observer<T>): Subscriber {
    if (typeof changeOrObserver === 'function') {
      return this.subscribeObserver({
        next: (value: T) => {
          changeOrObserver(value);
        },
      });
    } else {
      return this.subscribeObserver(changeOrObserver);
    }
  }

  /**
   * Subscribe to changes with an Observer.
   * This API was added in later, and provided as a separate function for Glic
   * API discovery purposes.
   */
  subscribeObserver(observer: Observer<T>): Subscriber {
    switch (this.state_) {
      case 'active':
        break;
      case 'complete':
        observer.complete?.();
        return {unsubscribe: () => {}};
      case 'error':
        observer.error?.(this.errorValue!);
        return {unsubscribe: () => {}};
    }
    const newSub =
        new ObservableSubscription(observer, this.onUnsubscribe.bind(this));
    if (this.subscribers.size === 0) {
      this.activeSubscriptionChanged(true);
    }
    this.subscribers.add(newSub);
    this.subscriberAdded(newSub);
    return newSub;
  }

  protected onUnsubscribe(sub: ObservableSubscription<T>) {
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

  protected subscriberAdded(_sub: ObservableSubscription<T>): void {}

  protected activeSubscriptionChanged(_hasActiveSubscription: boolean): void {}

  hasActiveSubscription(): boolean {
    return this.subscribers.size > 0;
  }
}

/**
 * A simple observable with no memory of previous values.
 */
export class Subject<T> extends ObservableBase<T> {
  override next(value: T): void {
    super.next(value);
  }
}

/**
 * A observable value that can change over time. If value is initialized, sends
 * it to new subscribers upon subscribe().
 */
export class ObservableValue<T> extends Subject<T> {
  protected constructor(
      private isSet: boolean, private value?: T,
      private hasActiveSubscriptionCallback?:
          (hasActiveSubscription: boolean) => void) {
    super();
  }

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

  /**
   * Assigns a new value to the ObservableValue and signals all subscribers.
   * Does nothing if the value is unchanged.
   */
  assignAndSignal(v: T, force = false) {
    if (this.isStopped()) {
      throw new Error('ObservableValue is not active');
    }
    const send = !this.isSet || this.value !== v || force;
    this.isSet = true;
    this.value = v;
    if (!send) {
      return;
    }
    super.next(v);
  }

  /** Returns the current value, or undefined if not initialized. */
  getCurrentValue(): T|undefined {
    return this.value;
  }

  /**
   * Asynchronously waits until the ObservableValue's current value satisfies a
   * given criteria.
   */
  async waitUntil(criteria: (value: T) => boolean): Promise<T> {
    const {promise, resolve, reject} = Promise.withResolvers<T>();
    const sub = this.subscribe({
      next(newValue) {
        if (criteria(newValue)) {
          resolve(newValue);
        }
      },
      error: reject,
      complete() {
        reject(new Error('Observable completed'));
      },
    });
    let resultValue: T;
    try {
      resultValue = await promise;
    } finally {
      sub.unsubscribe();
    }
    return resultValue;
  }

  protected override subscriberAdded(sub: ObservableSubscription<T>): void {
    if (this.isSet) {
      sub.observer.next?.(this.value!);
    }
  }

  protected override activeSubscriptionChanged(hasActiveSubscription: boolean):
      void {
    super.activeSubscriptionChanged(hasActiveSubscription);
    this.hasActiveSubscriptionCallback?.(hasActiveSubscription);
  }
}
