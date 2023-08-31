// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Type declarations for the chrome namespace.  This exposes
 * the private API to the extension.
 */

/**
 * The definition here is a clone from chrome private api.
 */
declare namespace chrome.wmDesksPrivate {
  export interface Desk {
    deskUuid: string;
    deskName: string;
  }
  export interface RemoveDeskOptions {
    combineDesks?: boolean;
    allowUndo?: boolean;
  }
  export interface LaunchOptions {
    templateUuid: string;
    deskName: string;
  }
  export interface WindowProperties {
    allDesks: boolean;
  }

  export function launchDesk(
      deskUuid: LaunchOptions, callback: DeskAddCallback): void;
  // Removes a desk as specified in `deskId`. If `combineDesks` of
  // `RemoveDeskOptions` is present or set to true, remove the desk and combine
  // windows to the active desk to the left. Otherwise close all windows on
  // the desk.
  export function removeDesk(
      deskUuid: string, options: RemoveDeskOptions,
      callback: VoidCallback): void;

  // Set the window properties for window identified by the `windowId`.
  export function setWindowProperties(
      windowId: number, windowProperties: WindowProperties,
      callback: VoidCallback): void;

  // Retrieves the UUID of the current active desk.
  export function getActiveDesk(callback: DeskIdCallback): void;

  // Switches to the target desk.
  export function switchDesk(deskUuid: string, callback: VoidCallback): void;

  // Retrieves desk information by its ID.
  export function getDeskByID(
      deskUuid: string, callback: GetDeskByIdCallback): void;

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  namespace OnDeskAdded {
    export function addListener(callback: DeskAddCallback): void;
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  namespace OnDeskRemoved {
    export function addListener(callback: DeskIdCallback): void;
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  namespace OnDeskSwitched {
    export function addListener(callback: DeskSwitchCallback): void;
  }

  export type VoidCallback = () => void;

  export type DeskIdCallback = (deskId: string) => void;

  export type GetDeskByIdCallback = (desk: Desk) => void;

  export type DeskSwitchCallback = (activated: string, deactivated: string) =>
      void;

  export type DeskAddCallback = (deskId: string, fromUndo: boolean) => void;
}
