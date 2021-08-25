// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * Events service interface exposed to third-parties to (un)subscribe on
 * different system event types.
 */

(() => {
  const messagePipe = dpsl.internal.messagePipe;


  /**
   * @enum {string}
   */
  const EVENTS = {
    AC_INSERTED: 'ac-inserted',
    AC_REMOVED: 'ac-removed',
    BLUETOOTH_ADAPTER_ADDED: 'bluetooth-adapter-added',
    BLUETOOTH_ADAPTER_PROPERTY_CHANGED: 'bluetooth-adapter-property-changed',
    BLUETOOTH_ADAPTER_REMOVED: 'bluetooth-adapter-removed',
    BLUETOOTH_DEVICE_ADDED: 'bluetooth-device-added',
    BLUETOOTH_DEVICE_PROPERTY_CHANGED: 'bluetooth-device-property-changed',
    BLUETOOTH_DEVICE_REMOVED: 'bluetooth-device-removed',
    LID_CLOSED: 'lid-closed',
    LID_OPENED: 'lid-opened',
    OS_RESUME: 'os-resume',
    OS_SUSPEND: 'os-suspend'
  }

  /**
   * Internal event target to deliver events on subscribtion.
   * @private
   * @suppress {checkTypes} closure compiler thinks EventTarget is an interface.
   */
  class InternalEventTarget extends EventTarget {
    constructor() {
      super();
      messagePipe.registerHandler(
          dpsl_internal.Message.DPSL_EVENTS_SERVICE_EVENTS, (message) => {
            const event = /** @type {!dpsl_internal.Event} */ (message);
            this.dispatchEvent(new Event(event.type));
          });
    }
  }

  /**
   * Bluetooth observer for dpsl.system_events.bluetooth.* APIs.
   */
  class BluetoothObserver {
    /**
     * @param {!InternalEventTarget} eventTarget
     */
    constructor(eventTarget) {
      /**
       * @type {!InternalEventTarget}
       * @const
       * @private
       */
      this.eventTarget = eventTarget;
    }

    /**
     * Starts listening on bluetooth-adapter-added events.
     * @param {function()} callback
     * @public
     */
    addOnAdapterAddedListener(callback) {
      this.eventTarget.addEventListener(
          EVENTS.BLUETOOTH_ADAPTER_ADDED, callback);
    }

    /**
     * Starts listening on bluetooth-adapter-property-changed events.
     * @param {function()} callback
     * @public
     */
    addOnAdapterPropertyChangedListener(callback) {
      this.eventTarget.addEventListener(
          EVENTS.BLUETOOTH_ADAPTER_PROPERTY_CHANGED, callback);
    }

    /**
     * Starts listening on bluetooth-adapter-removed events.
     * @param {function()} callback
     * @public
     */
    addOnAdapterRemovedListener(callback) {
      this.eventTarget.addEventListener(
          EVENTS.BLUETOOTH_ADAPTER_REMOVED, callback);
    }

    /**
     * Starts listening on bluetooth-device-added events.
     * @param {function()} callback
     * @public
     */
    addOnDeviceAddedListener(callback) {
      this.eventTarget.addEventListener(
          EVENTS.BLUETOOTH_DEVICE_ADDED, callback);
    }

    /**
     * Starts listening on bluetooth-device-property-changed events.
     * @param {function()} callback
     * @public
     */
    addOnDevicePropertyChangedListener(callback) {
      this.eventTarget.addEventListener(
          EVENTS.BLUETOOTH_DEVICE_PROPERTY_CHANGED, callback);
    }

    /**
     * Starts listening on bluetooth-device-removed events.
     * @param {function()} callback
     * @public
     */
    addOnDeviceRemovedListener(callback) {
      this.eventTarget.addEventListener(
          EVENTS.BLUETOOTH_DEVICE_REMOVED, callback);
    }

    /**
     * Stops listening on bluetooth-adapter-added events.
     * @param {function()} callback
     * @public
     */
    removeOnAdapterAddedListener(callback) {
      this.eventTarget.removeEventListener(
          EVENTS.BLUETOOTH_ADAPTER_ADDED, callback);
    }

    /**
     * Stops listening on bluetooth-adapter-property-changed events.
     * @param {function()} callback
     * @public
     */
    removeOnAdapterPropertyChangedListener(callback) {
      this.eventTarget.removeEventListener(
          EVENTS.BLUETOOTH_ADAPTER_PROPERTY_CHANGED, callback);
    }

    /**
     * Stops listening on bluetooth-adapter-removed events.
     * @param {function()} callback
     * @public
     */
    removeOnAdapterRemovedListener(callback) {
      this.eventTarget.removeEventListener(
          EVENTS.BLUETOOTH_ADAPTER_REMOVED, callback);
    }

    /**
     * Stops listening on bluetooth-device-added events.
     * @param {function()} callback
     * @public
     */
    removeOnDeviceAddedListener(callback) {
      this.eventTarget.removeEventListener(
          EVENTS.BLUETOOTH_DEVICE_ADDED, callback);
    }

    /**
     * Stops listening on bluetooth-device-property-changed events.
     * @param {function()} callback
     * @public
     */
    removeOnDevicePropertyChangedListener(callback) {
      this.eventTarget.removeEventListener(
          EVENTS.BLUETOOTH_DEVICE_PROPERTY_CHANGED, callback);
    }

    /**
     * Stops listening on bluetooth-device-removed events.
     * @param {function()} callback
     * @public
     */
    removeOnDeviceRemovedListener(callback) {
      this.eventTarget.removeEventListener(
          EVENTS.BLUETOOTH_DEVICE_REMOVED, callback);
    }
  }

  /**
   * Lid events observer for dpsl.system_events.lid.* APIs.
   */
  class LidObserver {
    /**
     * @param {!InternalEventTarget} eventTarget
     */
    constructor(eventTarget) {
      /**
       * @type {!InternalEventTarget}
       * @const
       * @private
       */
      this.eventTarget = eventTarget;
    }

    /**
     * Starts listening on lid-closed events.
     * @param {function()} callback
     * @public
     */
    addOnLidClosedListener(callback) {
      this.eventTarget.addEventListener(EVENTS.LID_CLOSED, callback);
    }
    /**
     * Starts listening on lid-opened events.
     * @param {function()} callback
     * @public
     */
    addOnLidOpenedListener(callback) {
      this.eventTarget.addEventListener(EVENTS.LID_OPENED, callback);
    }

    /**
     * Stops listening on lid-closed events.
     * @param {function()} callback
     * @public
     */
    removeOnLidClosedListener(callback) {
      this.eventTarget.removeEventListener(EVENTS.LID_CLOSED, callback);
    }
    /**
     * Stops listening on lid-opened events.
     * @param {function()} callback
     * @public
     */
    removeOnLidOpenedListener(callback) {
      this.eventTarget.removeEventListener(EVENTS.LID_OPENED, callback);
    }
  }

  /**
   * Power events observer for dpsl.system_events.power.* APIs.
   */
  class PowerObserver {
    /**
     * @param {!InternalEventTarget} eventTarget
     */
    constructor(eventTarget) {
      /**
       * @type {!InternalEventTarget}
       * @const
       * @private
       */
      this.eventTarget = eventTarget;
    }

    /**
     * Starts listening on ac-inserted events.
     * @param {function()} callback
     * @public
     */
    addOnAcInsertedListener(callback) {
      this.eventTarget.addEventListener(EVENTS.AC_INSERTED, callback);
    }

    /**
     * Starts listening on ac-removed events.
     * @param {function()} callback
     * @public
     */
    addOnAcRemovedListener(callback) {
      this.eventTarget.addEventListener(EVENTS.AC_REMOVED, callback);
    }
    /**
     * Starts listening on os-suspend events.
     * @param {function()} callback
     * @public
     */
    addOnOsSuspendListener(callback) {
      this.eventTarget.addEventListener(EVENTS.OS_SUSPEND, callback);
    }

    /**
     * Starts listening on os-resume events.
     * @param {function()} callback
     * @public
     */
    addOnOsResumeListener(callback) {
      this.eventTarget.addEventListener(EVENTS.OS_RESUME, callback);
    }

    /**
     * Stops listening on ac-inserted events.
     * @param {function()} callback
     * @public
     */
    removeOnAcInsertedListener(callback) {
      this.eventTarget.removeEventListener(EVENTS.AC_INSERTED, callback);
    }

    /**
     * Stops listening on ac-removed events.
     * @param {function()} callback
     * @public
     */
    removeOnAcRemovedListener(callback) {
      this.eventTarget.removeEventListener(EVENTS.AC_REMOVED, callback);
    }

    /**
     * Stops listening on os-suspend events.
     * @param {function()} callback
     * @public
     */
    removeOnOsSuspendListener(callback) {
      this.eventTarget.removeEventListener(EVENTS.OS_SUSPEND, callback);
    }

    /**
     * Stops listening on os-resume events.
     * @param {function()} callback
     * @public
     */
    removeOnOsResumeListener(callback) {
      this.eventTarget.removeEventListener(EVENTS.OS_RESUME, callback);
    }
  }

  /**
   * DPSL Events service for dpsl.system_events.* APIs.
   */
  class DPSLEventsService {
    constructor() {
      let eventTarget = new InternalEventTarget();

      /**
       * @type {!BluetoothObserver}
       * @public
       */
      this.bluetooth = new BluetoothObserver(eventTarget);

      /**
       * @type {!LidObserver}
       * @public
       */
      this.lid = new LidObserver(eventTarget);

      /**
       * @type {!PowerObserver}
       * @public
       */
      this.power = new PowerObserver(eventTarget);
    }

    /**
     * Returns the list of supported events.
     * @returns {!dpsl.EventTypes}
     * @public
     */
    getAvailableEvents() {
      return /** @type {dpsl.EventTypes} */ ([
        'ac-inserted', 'ac-removed', 'bluetooth-adapter-added',
        'bluetooth-adapter-property-changed', 'bluetooth-adapter-removed',
        'bluetooth-device-added', 'bluetooth-device-property-changed',
        'bluetooth-device-removed', 'lid-closed', 'lid-opened', 'os-resume',
        'os-suspend'
      ]);
    }
  }

  globalThis.dpsl.system_events = new DPSLEventsService();
})();
