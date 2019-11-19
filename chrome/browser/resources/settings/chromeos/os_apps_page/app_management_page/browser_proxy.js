// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  class BrowserProxy {
    constructor() {
      /** @type {appManagement.mojom.PageCallbackRouter} */
      this.callbackRouter = new appManagement.mojom.PageCallbackRouter();

      /** @type {appManagement.mojom.PageHandlerRemote} */
      this.handler = null;

      const urlParams = new URLSearchParams(window.location.search);
      const arcSupported = urlParams.get('arcSupported');
      const useFake = urlParams.get('fakeBackend');

      if (useFake) {
        if (arcSupported) {
          loadTimeData.overrideValues({
            'isSupportedArcVersion': arcSupported.toLowerCase() === 'true',
          });
        } else {
          loadTimeData.overrideValues({
            'isSupportedArcVersion': true,
          });
        }

        this.fakeHandler = new app_management.FakePageHandler(
            this.callbackRouter.$.bindNewPipeAndPassRemote());
        this.handler = this.fakeHandler.getRemote();

        const permissionOptions = {};
        permissionOptions[PwaPermissionType.GEOLOCATION] = {
          permissionValue: TriState.kAllow,
          isManaged: true,
        };
        permissionOptions[PwaPermissionType.MEDIASTREAM_CAMERA] = {
          permissionValue: TriState.kBlock,
          isManaged: true
        };

        const /** @type {!Array<App>}*/ appList = [
          app_management.FakePageHandler.createApp(
              'blpcfgokakmgnkcojhhkbfblekacnbeo',
              {
                title: 'Built in app, not implemented',
                type: AppType.kBuiltIn,
                installSource: InstallSource.kSystem,
              },
              ),
          app_management.FakePageHandler.createApp(
              'aohghmighlieiainnegkcijnfilokake',
              {
                title: 'Arc app',
                type: AppType.kArc,
                installSource: InstallSource.kUser,
              },
              ),
          app_management.FakePageHandler.createApp(
              'blpcfgokakmgnkcojhhkbfbldkacnbeo',
              {
                title: 'Crostini app, not implemented',
                type: AppType.kCrostini,
                installSource: InstallSource.kUser,
              },
              ),
          app_management.FakePageHandler.createApp(
              'pjkljhegncpnkkknowihdijeoejaedia',
              {
                title: 'Chrome App',
                type: AppType.kExtension,
                description:
                    'A Chrome App installed from the Chrome Web Store.',
              },
              ),
          app_management.FakePageHandler.createApp(
              'aapocclcdoekwnckovdopfmtonfmgok',
              {
                title: 'Web App',
                type: AppType.kWeb,
              },
              ),
          app_management.FakePageHandler.createApp(
              'pjkljhegncpnkkknbcohdijeoejaedia',
              {
                title: 'Chrome App, OEM installed',
                type: AppType.kExtension,
                description: 'A Chrome App installed by an OEM.',
                installSource: InstallSource.kOem,
              },
              ),
          app_management.FakePageHandler.createApp(
              'aapocclcgogkmnckokdopfmhonfmgok',
              {
                title: 'Web App, policy applied',
                type: AppType.kWeb,
                isPinned: apps.mojom.OptionalBool.kTrue,
                isPolicyPinned: apps.mojom.OptionalBool.kTrue,
                installSource: apps.mojom.InstallSource.kPolicy,
                permissions:
                    app_management.FakePageHandler.createWebPermissions(
                        permissionOptions),
              },
              ),
        ];

        this.fakeHandler.setApps(appList);

      } else {
        this.handler = new appManagement.mojom.PageHandlerRemote();
        const factory = appManagement.mojom.PageHandlerFactory.getRemote();
        factory.createPageHandler(
            this.callbackRouter.$.bindNewPipeAndPassRemote(),
            this.handler.$.bindNewPipeAndPassReceiver());
      }
    }
  }

  cr.addSingletonGetter(BrowserProxy);

  return {BrowserProxy: BrowserProxy};
});
