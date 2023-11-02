// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import '/file_path.mojom-lite.js';
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
 * Type alias for the UpdateState.
 * @typedef {ash.firmwareUpdate.mojom.UpdateState}
 */
export const UpdateState = ash.firmwareUpdate.mojom.UpdateState;

/**
 * Type alias for the UpdateProgressObserver.
 * @typedef {ash.firmwareUpdate.mojom.UpdateProgressObserver}
 */
export const UpdateProgressObserver =
    ash.firmwareUpdate.mojom.UpdateProgressObserver;

/**
 * Type alias for UpdateProgressObserverRemote.
 * @typedef {ash.firmwareUpdate.mojom.UpdateProgressObserverRemote}
 */
export const UpdateProgressObserverRemote =
    ash.firmwareUpdate.mojom.UpdateProgressObserverRemote;

/**
 * Type alias for InstallController.
 * @typedef {ash.firmwareUpdate.mojom.InstallController}
 */
export const InstallController = ash.firmwareUpdate.mojom.InstallController;

/**
 * Type alias for the InstallControllerInterface.
 * @typedef {ash.firmwareUpdate.mojom.InstallControllerInterface}
 */
export const InstallControllerInterface =
    ash.firmwareUpdate.mojom.InstallControllerInterface;

/**
 * Type alias for the UpdateProgressObserverInterface.
 * @typedef {ash.firmwareUpdate.mojom.UpdateProgressObserverInterface}
 */
export const UpdateProgressObserverInterface =
    ash.firmwareUpdate.mojom.UpdateProgressObserverInterface;

/**
 * Type for methods needed for the fake UpdateProvider implementation.
 * @typedef {{
 *   setDeviceIdForUpdateInProgress: !function(string),
 * }}
 */
export let FakeInstallControllerInterface;

/**
 * Type alias for InstallControllerRemote.
 * @typedef {ash.firmwareUpdate.mojom.InstallControllerRemote}
 */
export const InstallControllerRemote =
    ash.firmwareUpdate.mojom.InstallControllerRemote;

/**
 * Type alias for UpdateProgressObserverReceiver.
 * @typedef {ash.firmwareUpdate.mojom.UpdateProgressObserverReceiver}
 */
export const UpdateProgressObserverReceiver =
    ash.firmwareUpdate.mojom.UpdateProgressObserverReceiver;

/**
 * Type of UpdateControllerInterface.startUpdateFunction function.
 * @typedef {!function(string, !UpdateProgressObserver): void}
 */
export let startUpdateFunction;

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

/**
 * Type alias for InstallationProgress.
 * @typedef {ash.firmwareUpdate.mojom.InstallationProgress}
 */
export const InstallationProgress =
    ash.firmwareUpdate.mojom.InstallationProgress;

/**
 * @typedef {{
 *   title: string,
 *   body: string,
 *   footer: string,
 * }}
 */
export let DialogContent;
