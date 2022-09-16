// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/image_info.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/bitmap.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import '/app-management/file_path.mojom-lite.js';
import '/app-management/image.mojom-lite.js';
import '/app-management/safe_base_name.mojom-lite.js';
import '/app-management/types.mojom-lite.js';
import '/app-management/app_management.mojom-lite.js';

import {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy as ComponentBrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppType, InstallReason, OptionalBool} from 'chrome://resources/cr_components/app_management/constants.js';

import {FakePageHandler} from './fake_page_handler.js';

/** @type {?BrowserProxy} */
let instance = null;

export class BrowserProxy {
  /** @return {!BrowserProxy} */
  static getInstance() {
    return instance || (instance = new BrowserProxy());
  }

  /** @param {!BrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  constructor() {
    /** @type {appManagement.mojom.PageCallbackRouter} */
    this.callbackRouter = new appManagement.mojom.PageCallbackRouter();

    /** @type {appManagement.mojom.PageHandlerRemote} */
    this.handler = null;

    const urlParams = new URLSearchParams(window.location.search);
    const useFake = urlParams.get('fakeBackend');

    if (useFake) {
      this.fakeHandler =
          new FakePageHandler(this.callbackRouter.$.bindNewPipeAndPassRemote());
      this.handler = this.fakeHandler.getRemote();

      const permissionOptions = {};
      permissionOptions[PermissionType.kLocation] = {
        permissionValue: TriState.kAllow,
        isManaged: true,
      };
      permissionOptions[PermissionType.kCamera] = {
        permissionValue: TriState.kBlock,
        isManaged: true,
      };

      const /** @type {!Array<App>}*/ appList = [
        FakePageHandler.createApp(
            'blpcfgokakmgnkcojhhkbfblekacnbeo',
            {
              title: 'Built in app, not implemented',
              type: AppType.kBuiltIn,
              installReason: InstallReason.kSystem,
            },
            ),
        FakePageHandler.createApp(
            'aohghmighlieiainnegkcijnfilokake',
            {
              title: 'Arc app',
              type: AppType.kArc,
              installReason: InstallReason.kUser,
            },
            ),
        FakePageHandler.createApp(
            'blpcfgokakmgnkcojhhkbfbldkacnbeo',
            {
              title: 'Crostini app, not implemented',
              type: AppType.kCrostini,
              installReason: InstallReason.kUser,
            },
            ),
        FakePageHandler.createApp(
            'pjkljhegncpnkkknowihdijeoejaedia',
            {
              title: 'Chrome App',
              type: AppType.kChromeApp,
              description: 'A Chrome App installed from the Chrome Web Store.',
            },
            ),
        FakePageHandler.createApp(
            'aapocclcdoekwnckovdopfmtonfmgok',
            {
              title: 'Web App',
              type: AppType.kWeb,
            },
            ),
        FakePageHandler.createApp(
            'pjkljhegncpnkkknbcohdijeoejaedia',
            {
              title: 'Chrome App, OEM installed',
              type: AppType.kChromeApp,
              description: 'A Chrome App installed by an OEM.',
              installReason: InstallReason.kOem,
            },
            ),
        FakePageHandler.createApp(
            'aapocclcgogkmnckokdopfmhonfmgok',
            {
              title: 'Web App, policy applied',
              type: AppType.kWeb,
              isPinned: OptionalBool.kTrue,
              isPolicyPinned: OptionalBool.kTrue,
              installReason: InstallReason.kPolicy,
              permissions:
                  FakePageHandler.createWebPermissions(permissionOptions),
            },
            ),
      ];

      this.fakeHandler.setApps(appList);

    } else {
      this.handler = ComponentBrowserProxy.getInstance().handler;
      this.callbackRouter = ComponentBrowserProxy.getInstance().callbackRouter;
    }
  }
}
