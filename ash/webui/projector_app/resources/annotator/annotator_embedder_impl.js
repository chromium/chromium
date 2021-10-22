// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ProjectorBrowserProxyImpl} from '../../communication/projector_browser_proxy.js';

import {AnnotatorTrustedCommFactory, UntrustedAnnotatorClient} from './trusted/trusted_annotator_comm_factory.js';

Polymer({
  is: 'annotator-embedder-impl',

  behaviors: [WebUIListenerBehavior],

  /** @override */
  ready() {
    let client = AnnotatorTrustedCommFactory.getPostMessageAPIClient();

    this.addWebUIListener('undo', () => {
      client.undo();
    });

    this.addWebUIListener('redo', () => {
      client.redo();
    });

    this.addWebUIListener('clear', () => {
      client.clear();
    });

    this.addWebUIListener('setTool', async (tool) => {
      const success = await client.setTool(tool);
      if (success) {
        ProjectorBrowserProxyImpl.getInstance().onToolSet(tool);
      }
    });
  },
});
