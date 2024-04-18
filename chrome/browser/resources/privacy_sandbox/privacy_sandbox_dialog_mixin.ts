// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type { PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
// clang-format on

type Constructor<T> = new (...args: any[]) => T;

export const PrivacySandboxDialogMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PrivacySandboxDialogMixinInterface> => {
      class PrivacySandboxDialogMixin extends superClass {
        wasScrolledToBottom: boolean = true;

        private didStartWithScrollbar_: boolean = false;
        private wasScrolledToBottomResolver_: PromiseResolver<void>;

        static get properties() {
          return {
            wasScrolledToBottom: {
              type: Boolean,
              observer: 'onWasScrolledToBottomChange_',
            },
          };
        }

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

        onConsentMoreClicked() {
          this.onMoreClicked_();
          this.promptActionOccurred(
              PrivacySandboxPromptAction.CONSENT_MORE_BUTTON_CLICKED);
        }

        onNoticeMoreClicked() {
          this.onMoreClicked_();
          this.promptActionOccurred(
              PrivacySandboxPromptAction.NOTICE_MORE_BUTTON_CLICKED);
        }

        onRestrictedNoticeMoreClicked() {
          this.onMoreClicked_();
          this.promptActionOccurred(
              PrivacySandboxPromptAction.RESTRICTED_NOTICE_MORE_BUTTON_CLICKED);
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

        updateScrollableContents() {
          requestAnimationFrame(() => {
            const scrollable =
                this.shadowRoot!.querySelector<HTMLElement>('[scrollable]')!;
            scrollable.classList.toggle(
                'can-scroll',
                scrollable.clientHeight < scrollable.scrollHeight);
          });
        }

        // Checks if #lastTextElement is in the viewport of the [scrollable]
        // element. |wasScrolledToBottom| represents if the content was fully
        // shown at least once.
        //
        // The implementation uses IntersectionObserver which works async. The
        // callback is callback for the initial intersection ration after
        // starting the observer and then whenever a threshold was passed. The
        // method returns a promise which can be used to wait for the initial
        // layout to be ready.
        //
        // Requirements and assumptions:
        //  * #lastTextElement is the last element that has to be shown for the
        // dialog content to be considered fully shown.
        //  *  [scrollable] is the parent of #lastTextElement and it is the
        // main scrollable element.
        //  * .more-content-available is applied to [scrollable] when
        //  |wasScrolledToBottom| is false.
        //  * When computing the intersection, it takes into account the button
        //  container height (64px).
        maybeShowMoreButton(): Promise<void> {
          this.wasScrolledToBottomResolver_ = new PromiseResolver();
          return new Promise<void>(resolve => {
            // Determine if the dialog is scrolled to the bottom (or isn't
            // scrollable at all) and update more overlay accordingly.
            const scrollable: HTMLElement =
                this.shadowRoot!.querySelector('[scrollable]')!;
            // Reset `wasScrolledToBottom` to false to have consistent
            // behaviour.
            this.wasScrolledToBottom = false;

            const buttonRowHeight = 64;
            const lastTextElement =
                scrollable.querySelector('#lastTextElement')!;

            const options = {
              root: scrollable,
              threshold: [1],
              // While "More" button is shown, the content area take up the
              // whole window and button row in shown in-line as part of the
              // content area. Offset the root bounds by button row height to
              // take it into account.
              rootMargin: `0px 0px -${buttonRowHeight}px 0px`,
            };
            const observer = new IntersectionObserver(entries => {
              assert(entries.length === 1);
              // We cannot check for intersectionRatio strictly equal to 1
              // because its value is sometimes reported with ~0.99 values
              // (see crbug.com/1020466): this can lead to a state where
              // the more button is always visible, with unclickable action
              // buttons covered by an overlay (b/299120185).
              this.wasScrolledToBottom = entries[0].intersectionRatio >= 0.99;

              // After the whole text content was visible at least once, stop
              // observing.
              if (this.wasScrolledToBottom) {
                this.wasScrolledToBottomResolver_.resolve();
                observer.disconnect();
              }

              // After the callback was called once, resolve the returned
              // promise to inform the caller that the initial layout is done.
              resolve();
            }, options);
            observer.observe(lastTextElement);
          });
        }

        whenWasScrolledToBottomForTest(): Promise<void> {
          return this.wasScrolledToBottomResolver_.promise;
        }

        private onWasScrolledToBottomChange_() {
          const scrollable: HTMLElement =
              this.shadowRoot!.querySelector('[scrollable]')!;
          scrollable.classList.toggle(
              'more-content-available', !this.wasScrolledToBottom);
        }

        private onMoreClicked_() {
          // Scroll to reveal next visible portion of the content.
          const scrollable: HTMLElement =
              this.shadowRoot!.querySelector('[scrollable]')!;
          scrollable.scrollBy({top: scrollable.clientHeight});
        }
      }

      return PrivacySandboxDialogMixin;
    });

export interface PrivacySandboxDialogMixinInterface {
  wasScrolledToBottom: boolean;

  onConsentLearnMoreExpandedChanged(newValue: boolean, oldValue: boolean): void;
  onNoticeLearnMoreExpandedChanged(newValue: boolean, oldValue: boolean): void;
  onNoticeOpenSettings(): void;
  onNoticeAcknowledge(): void;
  maybeShowMoreButton(): Promise<void>;
  whenWasScrolledToBottomForTest(): Promise<void>;
  promptActionOccurred(action: PrivacySandboxPromptAction): void;
  updateScrollableContents(): void;
}
