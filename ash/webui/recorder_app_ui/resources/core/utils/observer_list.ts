// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type Observer<T> = (val: T) => void;
export type Unsubscribe = () => void;

export class ObserverList<T> {
  private readonly observers: Array<Observer<T>> = [];

  subscribe(observer: Observer<T>): Unsubscribe {
    this.observers.push(observer);
    return () => {
      this.unsubscribe(observer);
    };
  }

  private unsubscribe(observer: Observer<T>) {
    const idx = this.observers.indexOf(observer);
    if (idx !== -1) {
      this.observers.splice(idx, 1);
    }
  }

  notify(val: T): void {
    // Do a copy of observers in case any observer calls subscribe /
    // unsubscribe during the callback.
    for (const observer of [...this.observers]) {
      try {
        observer(val);
      } catch (error) {
        // Subscribers shouldn't fail, here we only log and continue to all
        // other subscribers.
        console.error(error);
      }
    }
  }
}
