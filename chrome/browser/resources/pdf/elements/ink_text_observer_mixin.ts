// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TextAttributes} from '../constants.js';
import {Ink2Manager} from '../ink2_manager.js';

type Constructor<T> = new (...args: any[]) => T;

export const InkTextObserverMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<InkTextObserverMixinInterface> => {
  class InkTextObserverMixin extends superClass implements
      InkTextObserverMixinInterface {
    private tracker_: EventTracker = new EventTracker();

    override firstUpdated() {
      this.onTextAttributesChanged(
          Ink2Manager.getInstance().getCurrentTextAttributes());
    }

    override connectedCallback() {
      super.connectedCallback();
      this.tracker_.add(
          Ink2Manager.getInstance(), 'attributes-changed',
          (e: Event) => this.onTextAttributesChanged(
              (e as CustomEvent<TextAttributes>).detail));
    }

    override disconnectedCallback() {
      super.disconnectedCallback();
      this.tracker_.removeAll();
    }

    // Should be overridden by clients.
    onTextAttributesChanged(_attributes: TextAttributes) {
      assertNotReached();
    }
  }
  return InkTextObserverMixin;
};

export interface InkTextObserverMixinInterface {
  onTextAttributesChanged(attributes: TextAttributes): void;
}
