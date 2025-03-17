// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicApiBootMessage, GlicHostRegistry, WithGlicApi} from '../glic_api/glic_api.js';

/*
This code is intended to be used by a glic web client to connect to the glic
host. The real client will not use this code, but will use a copy that is
similar. This copy is used for testing only.
*/

/**
 * Creates a GlicHostRegistry by loading the glic API script from Chrome and
 * calling the `internalAutoGlicBoot` function. This should be called on or
 * before page load. The returned promise never resolves if a message from
 * Chrome is not received.
 */
export function createGlicHostRegistryOnLoad(): Promise<GlicHostRegistry> {
  return new Promise<GlicHostRegistry>((resolve) => {
    const messageHandler = (event: MessageEvent) => {
      // Important: only accept messages from Chrome browser's glic WebUI.
      if (event.origin !== 'chrome://glic' || event.source === null) {
        return;
      }
      const bootMessage = event.data as GlicApiBootMessage;
      if (bootMessage?.type === 'glic-bootstrap' && bootMessage.glicApiSource) {
        window.removeEventListener('message', messageHandler);
        const glicApiSource = bootMessage.glicApiSource;
        const scriptElement = document.createElement('script');
        scriptElement.text = glicApiSource;
        document.head.appendChild(scriptElement);
        const bootFunc = (window as WithGlicApi).internalAutoGlicBoot;
        if (!bootFunc) {
          throw new Error('Glic client import failed.');
        }
        resolve(bootFunc(event.source as WindowProxy));
      }
    };
    window.addEventListener('message', messageHandler);
  });
}
