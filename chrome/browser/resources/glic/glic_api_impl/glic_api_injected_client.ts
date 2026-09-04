// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicHostRegistry, GlicWebClient, WithGlicApi} from '../glic_api/glic_api.js';

import {GlicHostRegistryImpl} from './client/glic_api_client.js';
import {GlicApiHost} from './host/glic_api_host.js';
import {ERROR_CODEC, WebClientDef, WebClientHostDef} from './request_types.js';
import type {WebClient, WebClientHost} from './request_types.js';
import type {PostMessageHandler} from './transport/post_message_transport.js';
import {createDirectMessagingPair} from './transport/post_message_transport.js';

/*
This is bundled into a js file, and sent to the web client. It should be
directly executable in a <script> element, and therefore should not have any
exports.
*/

class InFrameGlicBoot {
  static createRegistry(): GlicHostRegistry {
    return {
      async registerWebClient(webClient: GlicWebClient): Promise<void> {
        // Note: A migration is underway to use mojo directly from the
        // web client code. In the interim, there is still the guise of a
        // client/host architecture here, but in fact the host and client are
        // directly coupled.
        const directPair = createDirectMessagingPair<WebClientHost, WebClient>(
            'glic_api',
            ERROR_CODEC,
            // client root handler will be bound by GlicBrowserHostImpl later,
            // with setMessageHandler. For now, no messages will be handled.
            undefined as unknown as PostMessageHandler<typeof WebClientDef>,
            // host root handler will be bound by GlicApiHost below, with
            // setMessageHandler. For now, no messages will be handled.
            undefined as unknown as PostMessageHandler<typeof WebClientHostDef>,
            WebClientDef,
            WebClientHostDef,
        );

        const hostApi = new GlicApiHost(
            directPair.host.rootRemote,
            directPair.host.router,
        );

        const updateZoom = () => {
          hostApi?.onZoomLevelChanged(
              Math.round((window.devicePixelRatio || 1.0) * 100) / 100);
        };
        window.visualViewport?.addEventListener('resize', updateZoom);
        window.addEventListener('resize', updateZoom);

        window.addEventListener('click', (e: MouseEvent) => {
          const target = (e.target as HTMLElement)?.closest?.('a');
          if (target && target.getAttribute('target') === '_blank' &&
              target.href) {
            e.preventDefault();
            hostApi?.openLinkInNewTab(target.href);
          }
        }, true);

        directPair.host.rootReceiver.setMessageHandler(
            hostApi.hostMessageHandler, WebClientHostDef);

        const clientHostRegistry =
            new GlicHostRegistryImpl(directPair, hostApi);
        return clientHostRegistry.registerWebClient(webClient);
      },
    };
  }
}

(window as WithGlicApi).internalAutoGlicBoot = (_windowProxy?: WindowProxy) =>
    InFrameGlicBoot.createRegistry();
