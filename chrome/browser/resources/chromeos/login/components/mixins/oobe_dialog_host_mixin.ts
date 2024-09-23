// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {traceFirstScreenShown} from '../../oobe_trace.js';

import {OobeBaseMixin, OobeBaseMixinInterface} from './oobe_base_mixin.js';

/**
 * @fileoverview
 * 'OobeDialogHostMixin' is a mixin for oobe-dialog containers to
 * match oobe-dialog behavior.
 */

type Constructor<T> = new (...args: any[]) => T;

export const OobeDialogHostMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<OobeDialogHostMixinInterface> => {
      const superClassBase = OobeBaseMixin(superClass);
      class OobeDialogHostMixinInternal extends superClassBase implements
          OobeDialogHostMixinInterface {
        /**
         * Trigger onBeforeShow for all children.
         */
        override onBeforeShow(...data: any[]): void {
          super.onBeforeShow(data);
          traceFirstScreenShown();

          /* Triggers onBeforeShow for descendants. */
          const dialogs =
              this.shadowRoot?.querySelectorAll(
                  'oobe-dialog,oobe-adaptive-dialog,oobe-content-dialog,' +
                  'gaia-dialog,oobe-loading-dialog') ||
              [];
          for (const dialog of dialogs) {
            // Trigger show() if element is an oobe-dialog
            if ('onBeforeShow' in dialog &&
                typeof dialog.onBeforeShow === 'function') {
              dialog.onBeforeShow(data);
            }
          }
        }
      }

      return OobeDialogHostMixinInternal;
    });

export interface OobeDialogHostMixinInterface extends OobeBaseMixinInterface {
  onBeforeShow(...data: any[]): void;
}
