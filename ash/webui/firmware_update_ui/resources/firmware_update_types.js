// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './mojom/firmware_update.mojom-lite.js';

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

/**
 * Type alias for UpdatePriority.
 * @typedef {ash.firmwareUpdate.mojom.UpdatePriority}
 */
export const UpdatePriority = ash.firmwareUpdate.mojom.UpdatePriority;

/**
 * Type alias for FirmwareUpdate.
 * @typedef {ash.firmwareUpdate.mojom.FirmwareUpdate}
 */
export const FirmwareUpdate = ash.firmwareUpdate.mojom.FirmwareUpdate;

/**
 * Type alias for UpdateObserver.
 * @typedef {ash.firmwareUpdate.mojom.UpdateObserver}
 */
export const UpdateObserver = ash.firmwareUpdate.mojom.UpdateObserver;

/**
 * Type alias for UpdateObserverRemote.
 * @typedef {ash.firmwareUpdate.mojom.UpdateObserverRemote}
 */
export const UpdateObserverRemote =
    ash.firmwareUpdate.mojom.UpdateObserverRemote;

/**
 * Type alias for UpdateProvider.
 * @typedef {ash.firmwareUpdate.mojom.UpdateProvider}
 */
export const UpdateProvider = ash.firmwareUpdate.mojom.UpdateProvider;

/**
 * Type alias for the UpdateProviderInterface.
 * @typedef {ash.firmwareUpdate.mojom.UpdateProviderInterface}
 */
export const UpdateProviderInterface =
    ash.firmwareUpdate.mojom.UpdateProviderInterface;

/**
 * @typedef {{
 *   status: string,
 *   percentage: number,
 * }}
 */
export let InstallationProgress;

/**
 * Type alias for UpdateProgressObserver.
 * @typedef {{
 *   onProgressChanged: !function(!InstallationProgress)
 * }}
 */
export let UpdateProgressObserver;

/**
 * Type of UpdateControllerInterface.startUpdateFunction function.
 * @typedef {!function(string, !UpdateProgressObserver): void}
 */
export let startUpdateFunction;

/**
 * Type alias for the UpdateControllerInterface.
 * TODO(michaelcheco): Replace with a real mojo type when implemented.
 * @typedef {{
 *   startUpdate: !startUpdateFunction,
 * }}
 */
export let UpdateControllerInterface;

/**
 * Type alias for UpdateObserverReceiver.
 * @typedef {ash.firmwareUpdate.mojom.UpdateObserverReceiver}
 */
export const UpdateObserverReceiver =
    ash.firmwareUpdate.mojom.UpdateObserverReceiver;

/**
 * Type alias for UpdateObserverInterface.
 * @typedef {ash.firmwareUpdate.mojom.UpdateObserverInterface}
 */
export const UpdateObserverInterface =
    ash.firmwareUpdate.mojom.UpdateObserverInterface;

/**
 * Type for methods needed for the fake UpdateProvider implementation.
 * @typedef {{
 *   setFakeFirmwareUpdates: !function(!Array<FirmwareUpdate>),
 *   triggerDeviceAddedObserver: !function(): void,
 * }}
 */
export let FakeUpdateProviderInterface;
