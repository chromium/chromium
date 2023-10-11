// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import './firmware_update.mojom-webui.js';

import {FirmwareUpdate, InstallControllerInterface, UpdateProviderInterface} from './firmware_update.mojom-webui.js';

/**
 * @fileoverview
 * Type definitions for firmware update for the mojo API.
 */

/**
 * Type for methods needed for the fake UpdateProvider implementation.
 */
export type FakeInstallControllerInterface = InstallControllerInterface&{
  setDeviceIdForUpdateInProgress(deviceId: string): void,
};

/**
 * Type for methods needed for the fake UpdateProvider implementation.
 */
export type FakeUpdateProviderInterface = UpdateProviderInterface&{
  setFakeFirmwareUpdates(updates: FirmwareUpdate[][]): void,
  triggerDeviceAddedObserver(): void,
};

export interface DialogContent {
  title: string;
  body: string;
  footer: string;
}


export interface OpenUpdateDialogEventDetail {
  update: FirmwareUpdate;
  inflight: boolean;
}

export interface OpenConfirmationDialogEventDetail {
  update: FirmwareUpdate;
}


export interface IronAnnounceEventDetail {
  text: string;
}
