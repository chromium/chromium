// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {AdapterInfo, DiscoverySessionRemote} from './adapter.mojom-webui.js';
import {AdapterProperty, getAdapterBroker} from './adapter_broker.js';
import type {AdapterBroker} from './adapter_broker.js';
import {AdapterPage} from './adapter_page.js';
import type {BluetoothInternalsHandlerRemote} from './bluetooth_internals.mojom-webui.js';
// <if expr="is_chromeos">
import {BluetoothInternalsHandler} from './bluetooth_internals.mojom-webui.js';
// </if>
import {DebugLogPage} from './debug_log_page.js';
import type {DeviceInfo} from './device.mojom-webui.js';
import {DeviceCollection} from './device_collection.js';
import {DeviceDetailsPage} from './device_details_page.js';
import {DevicesPage, ScanStatus} from './devices_page.js';
import {PageManager} from './page_manager.js';
import type {PageManagerObserver} from './page_manager.js';
import {Sidebar} from './sidebar.js';
import {showSnackbar, SnackbarType} from './snackbar.js';



// Expose for testing.
export let adapterBroker: AdapterBroker|null = null;

export const devices: DeviceCollection = new DeviceCollection([]);

export let sidebarObj: Sidebar|null = null;

export const pageManager = PageManager.getInstance();

let adapterPage: AdapterPage|null = null;
let devicesPage: DevicesPage|null = null;
let debugLogPage: DebugLogPage|null = null;

let discoverySession: DiscoverySessionRemote|null = null;

let userRequestedScanStop: boolean = false;

/**
 * Observer for page changes. Used to update page title header.
 */
class PageObserver implements PageManagerObserver {
  updateHistory(path: string) {
    window.location.hash = '#' + path;
  }

  /**
   * Sets the page title. Called by PageManager.
   */
  updateTitle(title: string) {
    document.querySelector('.page-title')!.textContent = title;
  }
}

/**
 * Removes DeviceDetailsPage with matching device |address|. The associated
 * sidebar item is also removed.
 */
function removeDeviceDetailsPage(address: string) {
  const id = 'devices/' + address.toLowerCase();
  sidebarObj!.removeItem(id);

  const deviceDetailsPage =
      pageManager.registeredPages.get(id) as DeviceDetailsPage | undefined;

  // The device details page does not necessarily exist, return early if it is
  // not found.
  if (!deviceDetailsPage) {
    return;
  }

  deviceDetailsPage.disconnect();
  deviceDetailsPage.pageDiv.parentNode!.removeChild(deviceDetailsPage.pageDiv);

  // Inform the devices page that the user is inspecting this device.
  // This will update the links in the device table.
  devicesPage!.setInspecting(
      deviceDetailsPage.deviceInfo, /*isInspecting=*/ false);

  pageManager.unregister(deviceDetailsPage);
}

/**
 * Creates a DeviceDetailsPage with the given |deviceInfo|, appends it to
 * '#page-container', and adds a sidebar item to show the new page. If a
 * page exists that matches |deviceInfo.address|, nothing is created and the
 * existing page is returned.
 */
function makeDeviceDetailsPage(deviceInfo: DeviceInfo): DeviceDetailsPage {
  const deviceDetailsPageId = 'devices/' + deviceInfo.address.toLowerCase();
  const existingPage = pageManager.registeredPages.get(deviceDetailsPageId);
  if (existingPage) {
    return existingPage as DeviceDetailsPage;
  }

  const pageSection = document.createElement('section');
  pageSection.hidden = true;
  pageSection.id = deviceDetailsPageId;
  getRequiredElement('page-container').appendChild(pageSection);

  const deviceDetailsPage =
      new DeviceDetailsPage(deviceDetailsPageId, deviceInfo);

  deviceDetailsPage.pageDiv.addEventListener(
      'infochanged', (event: CustomEvent<{info: DeviceInfo}>) => {
        devices.addOrUpdate(event.detail.info);
      });

  deviceDetailsPage.pageDiv.addEventListener(
      'forgetpressed', (event: CustomEvent<{address: string}>) => {
        pageManager.showPageByName(devicesPage!.name);
        removeDeviceDetailsPage(event.detail.address);
      });

  // Inform the devices page that the user is inspecting this device.
  // This will update the links in the device table.
  devicesPage!.setInspecting(deviceInfo, /*isInspecting=*/ true);
  pageManager.register(deviceDetailsPage);

  sidebarObj!.addItem({
    pageName: deviceDetailsPageId,
    text: deviceInfo.nameForDisplay,
  });

  deviceDetailsPage.connect();
  return deviceDetailsPage;
}

/**
 * Updates the DeviceDetailsPage with the matching device |address| and
 * redraws it.
 */
function updateDeviceDetailsPage(address: string) {
  const detailPageId = 'devices/' + address.toLowerCase();
  const page = pageManager.registeredPages.get(detailPageId);
  if (page) {
    (page as DeviceDetailsPage).redraw();
  }
}

function updateStoppedDiscoverySession() {
  devicesPage!.setScanStatus(ScanStatus.OFF);
  discoverySession = null;
}

function setupAdapterSystem(response: {info: AdapterInfo}) {
  assert(adapterBroker);
  adapterBroker.addEventListener('adapterchanged', (e: Event) => {
    const event = e as CustomEvent<{property: AdapterProperty, value: boolean}>;
    assert(adapterPage);

    const oldValue = adapterPage.adapterFieldSet.value as AdapterInfo;
    const newValue = Object.assign({}, oldValue);
    (newValue as any)[event.detail.property] = event.detail.value;
    adapterPage.setAdapterInfo(newValue);

    if (event.detail.property === AdapterProperty.POWERED) {
      devicesPage!.updatedScanButtonVisibility(event.detail.value);
    }

    if (event.detail.property === AdapterProperty.DISCOVERING &&
        !event.detail.value && !userRequestedScanStop && discoverySession) {
      updateStoppedDiscoverySession();
      showSnackbar(
          'Discovery session ended unexpectedly', SnackbarType.WARNING);
    }
  });

  assert(adapterPage);
  adapterPage.setAdapterInfo(response.info);

  adapterPage.pageDiv.addEventListener('refreshpressed', () => {
    adapterBroker!.getInfo().then(response => {
      if (response && response.info) {
        adapterPage!.setAdapterInfo(response.info);
      } else {
        console.error('Failed to fetch adapter info.');
      }
    });
  });

  // <if expr="is_chromeos">
  adapterPage.pageDiv.addEventListener('restart-bluetooth-click', () => {
    const restartBluetoothBtn =
        document.querySelector<HTMLButtonElement>('#restart-bluetooth-btn');
    assert(restartBluetoothBtn);
    restartBluetoothBtn.textContent = 'Restarting system Bluetooth..';
    BluetoothInternalsHandler.getRemote()
        .restartSystemBluetooth()
        .catch((e) => {
          console.error('Failed to restart system Bluetooth', e);
        })
        .finally(() => {
          restartBluetoothBtn.textContent = 'Restart system Bluetooth';
          restartBluetoothBtn.disabled = false;
        });
  });
  // </if>
}

function setupDeviceSystem(response: {devices: DeviceInfo[]}) {
  // Hook up device collection events.
  assert(adapterBroker);
  adapterBroker.addEventListener('deviceadded', (e: Event) => {
    const event = e as CustomEvent<{deviceInfo: DeviceInfo}>;
    devices.addOrUpdate(event.detail.deviceInfo);
    updateDeviceDetailsPage(event.detail.deviceInfo.address);
  });
  adapterBroker.addEventListener('devicechanged', (e: Event) => {
    const event = e as CustomEvent<{deviceInfo: DeviceInfo}>;
    devices.addOrUpdate(event.detail.deviceInfo);
    updateDeviceDetailsPage(event.detail.deviceInfo.address);
  });
  adapterBroker.addEventListener('deviceremoved', (e: Event) => {
    const event = e as CustomEvent<{deviceInfo: DeviceInfo}>;
    devices.remove(event.detail.deviceInfo);
    updateDeviceDetailsPage(event.detail.deviceInfo.address);
  });

  response.devices.forEach(devices.addOrUpdate, devices);

  assert(devicesPage);
  devicesPage.setDevices(devices);

  devicesPage.pageDiv.addEventListener(
      'inspectpressed', (event: CustomEvent<{address: string}>) => {
        const detailsPage = makeDeviceDetailsPage(
            devices.item(devices.getByAddress(event.detail.address))!);
        pageManager.showPageByName(detailsPage.name);
      });

  devicesPage.pageDiv.addEventListener(
      'forgetpressed', (event: CustomEvent<{address: string}>) => {
        pageManager.showPageByName(devicesPage!.name);
        removeDeviceDetailsPage(event.detail.address);
      });

  devicesPage.pageDiv.addEventListener('scanpressed', () => {
    if (discoverySession) {
      userRequestedScanStop = true;
      devicesPage!.setScanStatus(ScanStatus.STOPPING);

      discoverySession.stop().then(response => {
        if (response.success) {
          updateStoppedDiscoverySession();
          userRequestedScanStop = false;
          return;
        }

        devicesPage!.setScanStatus(ScanStatus.ON);
        showSnackbar('Failed to stop discovery session', SnackbarType.ERROR);
        userRequestedScanStop = false;
      });

      return;
    }

    devicesPage!.setScanStatus(ScanStatus.STARTING);
    adapterBroker!.startDiscoverySession()
        .then(session => {
          assert(session);
          discoverySession = session;

          discoverySession.onConnectionError.addListener(() => {
            updateStoppedDiscoverySession();
            showSnackbar('Discovery session ended', SnackbarType.WARNING);
          });

          devicesPage!.setScanStatus(ScanStatus.ON);
        })
        .catch(error => {
          devicesPage!.setScanStatus(ScanStatus.OFF);
          showSnackbar('Failed to start discovery session', SnackbarType.ERROR);
          console.error(error);
        });
  });
}

function setupPages(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
  sidebarObj = new Sidebar(getRequiredElement('sidebar'));
  getRequiredElement('menu-btn').addEventListener('click', () => {
    sidebarObj!.open();
  });
  pageManager.addObserver(sidebarObj);
  pageManager.addObserver(new PageObserver());

  devicesPage = new DevicesPage();
  pageManager.register(devicesPage);
  adapterPage = new AdapterPage();
  pageManager.register(adapterPage);
  debugLogPage = new DebugLogPage(bluetoothInternalsHandler);
  pageManager.register(debugLogPage);

  // Set up hash-based navigation.
  window.addEventListener('hashchange', () => {
    // If a user navigates and the page doesn't exist, do nothing.
    const pageName = window.location.hash.substr(1);
    // Device page names are invalid selectors for querySelector(), as they
    // contain "/" and ":".
    // eslint-disable-next-line no-restricted-properties
    if (document.getElementById(pageName)) {
      pageManager.showPageByName(pageName);
    }
  });

  if (!window.location.hash) {
    pageManager.showPageByName(adapterPage.name);
    return;
  }

  // Only the root pages are available on page load.
  pageManager.showPageByName(window.location.hash.split('/')[0]!.substr(1));
}

function showRefreshPageDialog() {
  (document.getElementById('refresh-page') as HTMLDialogElement).showModal();
}

function showNeedLocationServicesOnDialog(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
  const dialog =
      document.getElementById('need-location-services-on') as HTMLDialogElement;
  const servicesLink =
      document.getElementById('need-location-services-on-services-link') as
      HTMLLinkElement;
  servicesLink.onclick = () => {
    dialog.close();
    showRefreshPageDialog();
    bluetoothInternalsHandler.requestLocationServices();
  };
  dialog.showModal();
}

function showNeedLocationPermissionAndServicesOnDialog(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
  const dialog =
      document.getElementById('need-location-permission-and-services-on') as
      HTMLDialogElement;
  const servicesLink =
      document.getElementById(
          'need-location-permission-and-services-on-services-link') as
      HTMLLinkElement;
  servicesLink.onclick = () => {
    dialog.close();
    showRefreshPageDialog();
    bluetoothInternalsHandler.requestLocationServices();
  };
  const permissionLink =
      document.getElementById(
          'need-location-permission-and-services-on-permission-link') as
      HTMLLinkElement;
  permissionLink.onclick = () => {
    dialog.close();
    showRefreshPageDialog();
    bluetoothInternalsHandler.requestSystemPermissions();
  };
  dialog.showModal();
}

function showNeedNearbyDevicesPermissionDialog(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
  const dialog = document.getElementById('need-nearby-devices-permission') as
      HTMLDialogElement;
  const permissionLink =
      document.getElementById(
          'need-nearby-devices-permission-permission-link') as HTMLLinkElement;
  permissionLink.onclick = () => {
    dialog.close();
    showRefreshPageDialog();
    bluetoothInternalsHandler.requestSystemPermissions();
  };
  dialog.showModal();
}

function showNeedLocationPermissionDialog(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
  const dialog =
      document.getElementById('need-location-permission') as HTMLDialogElement;
  const permissionLink =
      document.getElementById('need-location-permission-permission-link') as
      HTMLLinkElement;
  permissionLink.onclick = () => {
    dialog.close();
    showRefreshPageDialog();
    bluetoothInternalsHandler.requestSystemPermissions();
  };
  dialog.showModal();
}

function showCanNotRequestPermissionsDialog() {
  (document.getElementById('can-not-request-permissions') as HTMLDialogElement)
      .showModal();
}

export function initializeViews(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote) {
  setupPages(bluetoothInternalsHandler);
  return getAdapterBroker(bluetoothInternalsHandler)
      .then(broker => {
        adapterBroker = broker;
      })
      .then(() => {
        return adapterBroker!.getInfo();
      })
      .then(setupAdapterSystem)
      .then(() => {
        return adapterBroker!.getDevices();
      })
      .then(setupDeviceSystem)
      .catch((error: Error) => {
        showSnackbar(error.message, SnackbarType.ERROR);
        console.error(error);
      });
}

/**
 * Check if the system has all the needed system permissions for using
 * bluetooth.
 * @param bluetoothInternalsHandler Mojo remote handler.
 * @param successCallback The callback to be called when the system has the
 *     permissions for using bluetooth.
 */
export async function checkSystemPermissions(
    bluetoothInternalsHandler: BluetoothInternalsHandlerRemote,
    successCallback: (handler: BluetoothInternalsHandlerRemote) => void) {
  const {
    needLocationPermission,
    needNearbyDevicesPermission,
    needLocationServices,
    canRequestPermissions,
  } = await bluetoothInternalsHandler.checkSystemPermissions();
  const havePermission =
      !needNearbyDevicesPermission && !needLocationPermission;
  // In order to access Bluetooth, Android S+ requires us to have Nearby Devices
  // permission, and older versions of Android require Location permission and
  // Location Services to be turned on. Other platforms shouldn't have any of
  // these fields set to true.
  if (havePermission) {
    if (needLocationServices) {
      showNeedLocationServicesOnDialog(bluetoothInternalsHandler);
    } else {
      successCallback(bluetoothInternalsHandler);
    }
  } else if (canRequestPermissions) {
    if (needLocationServices) {
      // If Location Services are needed we can assume we are on an Android
      // version lower S and so Location, rather than Nearby Devices permission,
      // is also needed.
      showNeedLocationPermissionAndServicesOnDialog(bluetoothInternalsHandler);
    } else if (needNearbyDevicesPermission) {
      showNeedNearbyDevicesPermissionDialog(bluetoothInternalsHandler);
    } else {
      showNeedLocationPermissionDialog(bluetoothInternalsHandler);
    }
  } else {
    showCanNotRequestPermissionsDialog();
  }
}
