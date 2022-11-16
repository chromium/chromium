// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const PrivacySandboxDialogNoticeMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrivacySandboxDialogNoticeMixinInterface> => {
      class PrivacySandboxDialogNoticeMixin extends superClass {
        static get properties() {
          return {
            expanded_: {
              type: Boolean,
              observer: 'onLearnMoreExpandedChanged_',
            },
          };
        }

        private onLearnMoreExpandedChanged_() {
          // TODO(crbug.com/1378703): Report learn more actions.
        }

        onNoticeOpenSettings() {
          this.promptActionOccurred(
              PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
        }

        onNoticeAcknowledge() {
          this.promptActionOccurred(
              PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
        }

        promptActionOccurred(action: PrivacySandboxPromptAction) {
          PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(
              action);
        }
      }

      return PrivacySandboxDialogNoticeMixin;
    });

export interface PrivacySandboxDialogNoticeMixinInterface {
  onNoticeOpenSettings(): void;
  onNoticeAcknowledge(): void;
  promptActionOccurred(action: PrivacySandboxPromptAction): void;
}
