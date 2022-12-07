// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App, PageCallbackRouter, PageHandlerInterface, PermissionType, PermissionValue, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy as ComponentBrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppType, InstallReason, OptionalBool} from 'chrome://resources/cr_components/app_management/constants.js';

import {FakePageHandler} from './fake_page_handler.js';

export interface PermissionOption {
  permissionValue: TriState;
  isManaged: boolean;
  value?: PermissionValue;
}

let instance: AppManagementBrowserProxy|null = null;

export class AppManagementBrowserProxy {
  static getInstance(): AppManagementBrowserProxy {
    return instance || (instance = new AppManagementBrowserProxy());
  }

  static setInstanceForTesting(obj: AppManagementBrowserProxy): void {
    instance = obj;
  }

  callbackRouter: PageCallbackRouter;
  fakeHandler: FakePageHandler;
  handler: PageHandlerInterface;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    const urlParams = new URLSearchParams(window.location.search);
    const useFake = urlParams.get('fakeBackend');

    if (useFake) {
      this.fakeHandler =
          new FakePageHandler(this.callbackRouter.$.bindNewPipeAndPassRemote());
      this.handler = this.fakeHandler.getRemote();

      const permissionOptions: Record<PermissionType, PermissionOption> = {};
      permissionOptions[PermissionType.kLocation] = {
        permissionValue: TriState.kAllow,
        isManaged: true,
      };
      permissionOptions[PermissionType.kCamera] = {
        permissionValue: TriState.kBlock,
        isManaged: true,
      };

      const appList: App[] = [
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
