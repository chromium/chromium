// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by Polymer elements that want to
 * automatically remove WebUI listeners when detached.
 */

import {addWebUiListener, removeWebUiListener, WebUiListener} from '//resources/js/cr.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const WebUiListenerMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<WebUiListenerMixinInterface> => {
      class WebUiListenerMixin extends superClass implements
          WebUiListenerMixinInterface {
        /**
         * Holds WebUI listeners that need to be removed when this element is
         * destroyed.
         */
        private webUiListeners_: WebUiListener[] = [];

        /**
         * Adds a WebUI listener and registers it for automatic removal when
         * this element is detached. Note: Do not use this method if you intend
         * to remove this listener manually (use addWebUiListener directly
         * instead).
         *
         * @param eventName The event to listen to.
         * @param callback The callback run when the event is fired.
         */
        addWebUiListener(eventName: string, callback: Function) {
          this.webUiListeners_.push(addWebUiListener(eventName, callback));
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          while (this.webUiListeners_.length > 0) {
            removeWebUiListener(this.webUiListeners_.pop()!);
          }
        }
      }
      return WebUiListenerMixin;
    });

export interface WebUiListenerMixinInterface {
  addWebUiListener(eventName: string, callback: Function): void;
}
