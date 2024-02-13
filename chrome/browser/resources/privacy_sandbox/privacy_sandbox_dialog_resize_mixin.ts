// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy} from './privacy_sandbox_dialog_browser_proxy.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const PrivacySandboxDialogResizeMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrivacySandboxDialogResizeMixinInterface> => {
      class PrivacySandboxDialogResizeMixin extends superClass implements
          PrivacySandboxDialogResizeMixinInterface {
        /**
         * Resize the native dialog to fit the rendered UI. After the dialog is
         * resized, fit the UI into the dialog bounds by applying 'fill-content'
         * class. This should be called once per dialog after the UI has
         * finished rendering.
         */
        resizeAndShowNativeDialog(): Promise<void> {
          return new Promise(async resolve => {
            const proxy = PrivacySandboxDialogBrowserProxy.getInstance();
            // Prefer using |document.body.offsetHeight| instead of
            // |document.body.scrollHeight| as it returns the correct height
            // of the page even when the page zoom in Chrome is different
            // than 100%.
            await proxy.resizeDialog(document.body.offsetHeight);

            // After the content was rendered at size it requires, toggle a
            // class to fit the content into native dialog bounds...
            const elements = this.shadowRoot!.querySelectorAll<HTMLElement>(
                '[fill-content]');
            for (const element of elements) {
              element.classList.toggle('fill-content', true);
            }

            // ...and hide any overflow on the body. 'fill-content' element
            // fills the dialog and any scrolling will be happening inside
            // it.
            document.body.style.overflow = 'hidden';

            // After the layout is adjusted to fit into the dialog, show
            // the native dialog.
            proxy.showDialog();
            resolve();
          });
        }
      }

      return PrivacySandboxDialogResizeMixin;
    });

export interface PrivacySandboxDialogResizeMixinInterface {
  resizeAndShowNativeDialog(): Promise<void>;
}
