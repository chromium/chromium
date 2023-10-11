// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import './firmware_update.mojom-webui.js';

import {FirmwareUpdate, UpdateProgressObserver} from './firmware_update.mojom-webui.js';

/**
 * @fileoverview
 * Type definitions for firmware update for the mojo API.
 */

/**
 * Type for methods needed for the fake UpdateProvider implementation.
 * @typedef {{
 *   setDeviceIdForUpdateInProgress: !function(string),
 * }}
 */
export let FakeInstallControllerInterface;

/**
 * Type of UpdateControllerInterface.startUpdateFunction function.
 * @typedef {!function(string, !UpdateProgressObserver): void}
 */
export let startUpdateFunction;

/**
 * Type for methods needed for the fake UpdateProvider implementation.
 * @typedef {{
 *   setFakeFirmwareUpdates: !function(!Array<FirmwareUpdate>),
 *   triggerDeviceAddedObserver: !function(): void,
 * }}
 */
export let FakeUpdateProviderInterface;

/**
 * @typedef {{
 *   title: string,
 *   body: string,
 *   footer: string,
 * }}
 */
export let DialogContent;

/**
 * @typedef {{
 *    update: FirmwareUpdate;
 *    inflight: boolean;
 * }}
 */
export let OpenUpdateDialogEventDetail;

/**
 * @typedef {{
 *    update: FirmwareUpdate;
 * }}
 */
export let OpenConfirmationDialogEventDetail;

/**
 * @typedef {{
 *    text: string;
 * }}
 */
export let IronAnnounceEventDetail;
