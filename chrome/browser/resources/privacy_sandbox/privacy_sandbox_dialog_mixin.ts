// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const PrivacySandboxDialogMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrivacySandboxDialogMixinInterface> => {
      class PrivacySandboxDialogMixin extends superClass {
        private didStartWithScrollbar_: boolean = false;

        onConsentLearnMoreExpandedChanged(
            newValue: boolean, oldValue: boolean) {
          // Check both old and new value to avoid reporting actions when the
          // dialog just was created and oldValue is undefined.
          if (newValue && !oldValue) {
            this.onContentSizeChanging_(/*expanding=*/ true);
            this.promptActionOccurred(
                PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
          }
          if (!newValue && oldValue) {
            this.onContentSizeChanging_(/*expanding=*/ false);
            this.promptActionOccurred(
                PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
          }
        }

        onNoticeLearnMoreExpandedChanged(newValue: boolean, oldValue: boolean) {
          // Check both old and new value to avoid reporting actions when the
          // dialog just was created and oldValue is undefined.
          if (newValue && !oldValue) {
            this.onContentSizeChanging_(/*expanding=*/ true);
            this.promptActionOccurred(
                PrivacySandboxPromptAction.NOTICE_MORE_INFO_OPENED);
          }
          if (!newValue && oldValue) {
            this.onContentSizeChanging_(/*expanding=*/ false);
            this.promptActionOccurred(
                PrivacySandboxPromptAction.NOTICE_MORE_INFO_CLOSED);
          }
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

        private onContentSizeChanging_(expanding: boolean) {
          const scrollable: HTMLElement|null =
              this.shadowRoot!.querySelector('[scrollable]');

          if (expanding) {
            // Always allow the scrollbar to show when the learn more is
            // expanded, but remember whether it was shown when learn more was
            // collapsed.
            scrollable!.classList.toggle('hide-scrollbar', false);
            this.didStartWithScrollbar_ =
                scrollable!.offsetHeight < scrollable!.scrollHeight;
          } else {
            // On collapse, return the scrollbar to the pre-expand state
            // immediately.
            scrollable!.classList.toggle(
                'hide-scrollbar', !this.didStartWithScrollbar_);
          }
        }
      }

      return PrivacySandboxDialogMixin;
    });

export interface PrivacySandboxDialogMixinInterface {
  onConsentLearnMoreExpandedChanged(newValue: boolean, oldValue: boolean): void;
  onNoticeLearnMoreExpandedChanged(newValue: boolean, oldValue: boolean): void;
  onNoticeOpenSettings(): void;
  onNoticeAcknowledge(): void;
  promptActionOccurred(action: PrivacySandboxPromptAction): void;
}
