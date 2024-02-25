// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Type declarations for Desk API
 */

/**
 * Operands for removing desks.
 */
export interface RemoveDeskOperands {
  deskId: string;
  options: RemoveDeskOptions;
  skipConfirmation?: boolean;
  confirmationSetting?: ConfirmationSetting;
}

/**
 * Operands for switching desks.
 */
export interface SwitchDeskOperands {
  deskId: string;
}

/**
 * Options for removing desks.
 */
export interface RemoveDeskOptions {
  combineDesks?: boolean;
  allowUndo?: boolean;
}

/**
 * Operands for retrieve desk information.
 */
export interface GetDeskByIdOperands {
  deskId: string;
}


/**
 * Confirmation window setting for desk removal.
 */
export interface ConfirmationSetting {
  title: string;
  message: string;
  iconUrl: string;
  acceptMessage: string;
  rejectMessage: string;
}

/**
 * Virtual desk.
 */
export interface Desk {
  deskUuid: string;
  deskName?: string;
}

/**
 * Launch new desk options.
 */
export interface LaunchOptions {
  templateUuid: string;
  deskName: string;
}

/**
 * Window Properties.
 */
export interface WindowProperties {
  allDesks: boolean;
}

/**
 * Notification window options.
 */
export interface NotificationOptions {
  title: string;
  message: string;
  iconUrl: string;
  buttons: Button[];
}

/**
 * Notification button.
 */
export interface Button {
  title: string;
}

/**
 * This interface provides the function signatures that will be provided
 * by both the mock and the Desk API. We can pass an implementation for this
 * interface to the service worker for our dependency injection.
 */
export interface DeskApi {
  launchDesk(options: LaunchOptions, callback: DeskIdCallback): void;
  removeDesk(
      deskUuId: string, options: RemoveDeskOptions,
      callback: VoidCallback): void;
  setWindowProperties(
      windowId: number, windowProperties: WindowProperties,
      callback: VoidCallback): void;
  getActiveDesk(callback: DeskIdCallback): void;
  switchDesk(deskId: string, callback: VoidCallback): void;
  getDeskById(deskId: string, callback: DeskCallback): void;
  addDeskAddedListener(callback: DeskAddCallback): void;
  addDeskRemovedListener(callback: DeskIdCallback): void;
  addDeskSwitchedListener(callback: DeskSwitchCallback): void;
}

/**
 * Interface for emitting notifications.
 */
export interface NotificationApi {
  create(
      notificationId: string, options: NotificationOptions,
      callback: VoidCallback): void;
  addClickEventListener(callback: ClickEventListener): void;
  clear(notificationId: string): void;
}

/**
 * Callback that takes empty params and return void.
 */
export type VoidCallback = () => void;

/**
 * Callback for desk Id output.
 */
export type DeskIdCallback = (deskId: string) => void;

/**
 * Callback for notification event listener.
 */
export type ClickEventListener =
    (notificationId: string, buttonIndex: number) => void;

/**
 * Callback for desk output.
 */
export type DeskCallback = (desk: Desk) => void;

/**
 * Callback for desk switch.
 */
export type DeskSwitchCallback = (activated: string, deactivated: string) =>
    void;

/**
 * Callback for desk add.
 */
export type DeskAddCallback = (deskId: string, fromUndo: boolean) => void;
