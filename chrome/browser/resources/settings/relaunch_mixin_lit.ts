// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {LifetimeBrowserProxy} from '/shared/settings/lifetime_browser_proxy.js';
import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';

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
export const RelaunchMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<RelaunchMixinLitInterface> => {
  class RelaunchMixinLit extends superClass implements
      RelaunchMixinLitInterface {
    static get properties() {
      return {
        shouldShowRelaunchDialog: {type: Boolean},
      };
    }

    accessor shouldShowRelaunchDialog: boolean = false;
    private lifetimeBrowserProxy_: LifetimeBrowserProxy;

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

    // <if expr="not is_chromeos">
    private async performRestartForNonChromeOs_(
        restartType: RestartType, alwaysShowDialog: boolean) {
      const shouldShowDialog =
          await this.lifetimeBrowserProxy_.shouldShowRelaunchConfirmationDialog(
              alwaysShowDialog);
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
     * @param alwaysShowDialog Always show a confirmation dialog before the
     *     restart if this parameter is true. Otherwise, only when there is
     *     an incognito window open.
     */
    performRestart(restartType: RestartType, alwaysShowDialog?: boolean) {
      if (alwaysShowDialog == null) {
        alwaysShowDialog = false;
      }

      // <if expr="is_chromeos">
      this.performRestartInternal_(restartType);
      // </if>

      // <if expr="not is_chromeos">
      this.performRestartForNonChromeOs_(restartType, alwaysShowDialog);
      // </if>
    }
  }
  return RelaunchMixinLit;
};

export interface RelaunchMixinLitInterface {
  shouldShowRelaunchDialog: boolean;
  onRelaunchDialogClose(event: Event): void;
  performRestart(restartType: RestartType, alwaysShowDialog?: boolean): void;
}
