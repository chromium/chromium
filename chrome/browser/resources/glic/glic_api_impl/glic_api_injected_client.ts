// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicHostRegistry, GlicWebClient, WithGlicApi} from '../glic_api/glic_api.js';

import {GlicHostRegistryImpl} from './client/glic_api_client.js';
import {GlicApiHost} from './host/glic_api_host.js';

/*
This is bundled into a js file, and sent to the web client. It should be
directly executable in a <script> element, and therefore should not have any
exports.
*/

class InFrameGlicBoot {
  static createRegistry(): GlicHostRegistry {
    return {
      async registerWebClient(webClient: GlicWebClient): Promise<void> {
        const hostApi = new GlicApiHost();

        window.addEventListener('click', (e: MouseEvent) => {
          const target = (e.target as HTMLElement)?.closest?.('a');
          if (target && target.getAttribute('target') === '_blank' &&
              target.href) {
            e.preventDefault();
            hostApi?.openLinkInNewTab(target.href);
          }
        }, true);

        const clientHostRegistry = new GlicHostRegistryImpl(hostApi);
        return clientHostRegistry.registerWebClient(webClient);
      },
    };
  }
}

(window as WithGlicApi).internalAutoGlicBoot = (_windowProxy?: WindowProxy) =>
    InFrameGlicBoot.createRegistry();
