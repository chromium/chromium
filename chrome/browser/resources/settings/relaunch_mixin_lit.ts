// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {LifetimeBrowserProxy} from '/shared/settings/lifetime_browser_proxy.js';
import {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
// <if expr="not is_chromeos">
import {assert} from 'chrome://resources/js/assert.js';

import type {RelaunchConfirmationDialogElement} from './relaunch_confirmation_dialog.js';
// </if>

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
    // <if expr="not is_chromeos">
    private restartType_: RestartType = RestartType.RESTART;
    private onBeforeRestart_: () => void = () => {};
    // </if>

    constructor(...args: any[]) {
      super(...args);
      this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
    }

    onRelaunchDialogClose(_event: Event) {
      this.shouldShowRelaunchDialog = false;
      // <if expr="not is_chromeos">
      const onBeforeRestart = this.onBeforeRestart_;
      this.onBeforeRestart_ = () => {};
      const dialog =
          this.shadowRoot.querySelector<RelaunchConfirmationDialogElement>(
              'relaunch-confirmation-dialog');
      assert(dialog);
      if (dialog.wasConfirmed()) {
        this.performRestartInternal_(this.restartType_, onBeforeRestart);
      }
      // </if>
    }

    private performRestartInternal_(
        restartType: RestartType, onBeforeRestart: () => void) {
      onBeforeRestart();
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
        restartType: RestartType, alwaysShowDialog: boolean,
        onBeforeRestart: () => void) {
      const shouldShowDialog =
          await this.lifetimeBrowserProxy_.shouldShowRelaunchConfirmationDialog(
              alwaysShowDialog);
      if (!shouldShowDialog) {
        this.performRestartInternal_(restartType, onBeforeRestart);
        return;
      }

      // Used by `onRelaunchDialogClose()` if the user confirms.
      this.restartType_ = restartType;
      this.onBeforeRestart_ = onBeforeRestart;
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
     * @param onBeforeRestart Called right before the restart, i.e. only once
     *     it's confirmed: not at all if the user cancels the
     *     <relaunch-confirmation-dialog>. Must be synchronous: the restart
     *     doesn't wait for anything it starts asynchronously.
     */
    performRestart(
        restartType: RestartType, alwaysShowDialog?: boolean,
        onBeforeRestart: () => void = () => {}) {
      if (alwaysShowDialog == null) {
        alwaysShowDialog = false;
      }

      // <if expr="is_chromeos">
      this.performRestartInternal_(restartType, onBeforeRestart);
      // </if>

      // <if expr="not is_chromeos">
      this.performRestartForNonChromeOs_(
          restartType, alwaysShowDialog, onBeforeRestart);
      // </if>
    }
  }
  return RelaunchMixinLit;
};

export interface RelaunchMixinLitInterface {
  shouldShowRelaunchDialog: boolean;
  onRelaunchDialogClose(event: Event): void;
  /**
   * `onBeforeRestart` must be synchronous: the restart doesn't wait for
   * anything it starts asynchronously. See `performRestart()`.
   */
  performRestart(
      restartType: RestartType, alwaysShowDialog?: boolean,
      onBeforeRestart?: () => void): void;
}
