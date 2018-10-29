// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *    name: string,
 *    permissions: !Array<string>
 * }}
 */
let Extension;

cr.define('management', function() {
  /**
   * A singleton object that handles communication between browser and WebUI.
   */
  class Page {
    constructor() {
      /** @private {!ManagementBrowserProxy} */
      this.browserProxy_ = ManagementBrowserProxyImpl.getInstance();
    }

    /**
     * Main initialization function. Called by the browser on page load.
     */
    initialize() {
      // Notify the browser that the page has loaded, causing it to send the
      // management data.

      // Show whether device is managed or not, and the management domain in the
      // former case.
      this.browserProxy_.getDeviceManagementStatus()
          .then(function(managedString) {
            $('management-status').hidden = false;
            document.body.querySelector('#management-status > p').textContent =
                managedString;
          })
          .catch(
              // On Chrome desktop, the behavior is to show nothing (device
              // management is outside of Chrome's control), so
              // RejectJavascriptCallback is used, which throws an error. The
              // intended handling in this case is to do nothing.
              () => {});

      // Show descriptions of the types of reporting in the |reportingSources|
      // list.
      this.browserProxy_.getReportingInfo().then(function(reportingSources) {
        if (reportingSources.length == 0)
          return;

        $('policies').hidden = false;

        for (const id of reportingSources) {
          const element = document.createElement('li');
          element.textContent = loadTimeData.getString(id);
          $('reporting-info-list').appendChild(element);
        }
      });

      // Show names and permissions of |extensions| in a table.
      this.browserProxy_.getExtensions().then(function(extensions) {
        if (extensions.length == 0)
          return;

        const table = $('extensions-table');

        for (const /** Extension */ extension of extensions) {
          assert(
              extension.hasOwnProperty('permissions'),
              'Each extension must have the permissions field');
          assert(
              extension.hasOwnProperty('name'),
              'Each extension must have the name field');

          const permissionsList = document.createElement('ul');
          for (const perm of extension.permissions) {
            const permissionElement = document.createElement('li');
            permissionElement.textContent = perm;
            permissionsList.appendChild(permissionElement);
          }

          const row = table.insertRow();
          const nameCell = row.insertCell();
          // insertCell(-1) inserts at the last position.
          const permissionsCell = row.insertCell(-1);
          nameCell.textContent = extension.name;
          permissionsCell.appendChild(permissionsList);
        }

        $('extensions').hidden = false;
      });
    }
  }

  /** @interface */
  class ManagementBrowserProxy {
    /**
     * @return {!Promise<string>} Message stating if device is enterprise
     * managed and by whom.
     */
    getDeviceManagementStatus() {}

    /**
     * @return {!Promise<!Array<string>>} Types of device reporting.
     */
    getReportingInfo() {}

    /**
     * Each extension has a name and a list of permission messages.
     * @return {!Promise<!Array<!Extension>>} List of extensions.
     */
    getExtensions() {}
  }

  /**
   * @implements {ManagementBrowserProxy}
   */
  class ManagementBrowserProxyImpl {
    /** @override */
    getDeviceManagementStatus() {
      return cr.sendWithPromise('getDeviceManagementStatus');
    }

    /** @override */
    getReportingInfo() {
      return cr.sendWithPromise('getReportingInfo');
    }

    /** @override */
    getExtensions() {
      return cr.sendWithPromise('getExtensions');
    }
  }

  // Make Page a singleton.
  cr.addSingletonGetter(Page);
  cr.addSingletonGetter(ManagementBrowserProxyImpl);

  return {Page: Page};
});

// Have the main initialization function be called when the page finishes
// loading.
document.addEventListener(
    'DOMContentLoaded', () => management.Page.getInstance().initialize());
