// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from './lifetime_browser_proxy.js';
// clang-format on

export enum RestartType {
  RESTART,
  RELAUNCH,
}

type Constructor<T> = new (...args: any[]) => T;

/**
 * A helper Mixin to channel the relaunch/restart signal to native Chrome.
 * This uses LifetimeBrowserProxy under the surface but additionally supports
 * the <relaunch-confirmation-dialog> for non ChromeOS based desktop platforms.
 */
export const RelaunchMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RelaunchMixinInterface> => {
      class RelaunchMixin extends superClass {
        private lifetimeBrowserProxy_: LifetimeBrowserProxy;

        static get properties() {
          return {
            shouldShowRelaunchDialog: {
              type: Boolean,
              value: false,
            },

            restartTypeEnum: {
              type: Object,
              value: RestartType,
            },
          };
        }

        protected shouldShowRelaunchDialog: boolean;

        constructor(...args: any[]) {
          super(...args);
          this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
        }

        onRelaunchDialogClose(_event: Event) {
          this.shouldShowRelaunchDialog = false;
        }

        private performRestartInternal_(restartType: RestartType) {
          if (RestartType.RESTART === restartType) {
            this.lifetimeBrowserProxy_.restart();
          } else if (RestartType.RELAUNCH === restartType) {
            this.lifetimeBrowserProxy_.relaunch();
          } else {
            assertNotReached();
          }
        }

        // <if expr="not chromeos_ash">
        private async performRestartForNonChromeOs_(restartType: RestartType) {
          const shouldShowDialog = await this.lifetimeBrowserProxy_
                                       .shouldShowRelaunchConfirmationDialog();
          if (!shouldShowDialog) {
            this.performRestartInternal_(restartType);
            return;
          }

          this.shouldShowRelaunchDialog = true;
        }
        // </if>

        /**
         * This either performs restart or relaunch depending on the function
         * argument restartType. For non ChromeOS platforms it shows the
         * additional <relaunch-confirmation-dialog> html element **if** that
         * was specified in the caller's DOM, **otherwise** doesn't do anything.
         * Please see, RelaunchConfirmationDialogElement for more information on
         * how to add the new <relaunch-confirmation-dialog> element in the DOM.
         *
         * @param restartType This specifies the type of restart to perform.
         */
        performRestart(restartType: RestartType) {
          // <if expr="chromeos_ash">
          this.performRestartInternal_(restartType);
          // </if>

          // <if expr="not chromeos_ash">
          this.performRestartForNonChromeOs_(restartType);
          // </if>
        }
      }
      return RelaunchMixin;
    });


export interface RelaunchMixinInterface {
  performRestart(restartType: RestartType): void;
}
