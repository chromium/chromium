// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ProjectorBrowserProxyImpl} from '../../communication/projector_browser_proxy.js';

import {AnnotatorTrustedCommFactory, UntrustedAnnotatorClient} from './trusted/trusted_annotator_comm_factory.js';

/**
 * Enum for passing annotator error message to the browser process.
 * @enum {string}
 */
const AnnotatorToolErrorType = {
  UNDO_ERROR: 'UNDO_ERROR',
  REDO_ERROR: 'REDO_ERROR',
  CLEAR_ERROR: 'CLEAR_ERROR',
  SET_TOOL_ERROR: 'SET_TOOL_ERROR',
};

Polymer({
  is: 'annotator-embedder-impl',

  behaviors: [WebUIListenerBehavior],

  /** @override */
  ready() {
    const client = AnnotatorTrustedCommFactory.getPostMessageAPIClient();

    this.addWebUIListener('undo', () => {
      try {
        client.undo();
      } catch (error) {
        ProjectorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.UNDO_ERROR]);
      }
    });

    this.addWebUIListener('redo', () => {
      try {
        client.redo();
      } catch (error) {
        ProjectorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.REDO_ERROR]);
      }
    });

    this.addWebUIListener('clear', () => {
      try {
        client.clear();
      } catch (error) {
        ProjectorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.CLEAR_ERROR]);
      }
    });

    this.addWebUIListener('setTool', async (tool) => {
      try {
        const success = await client.setTool(tool);
        if (success) {
          ProjectorBrowserProxyImpl.getInstance().onToolSet(tool);
        }
      } catch (error) {
        ProjectorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.SET_TOOL_ERROR]);
      }
    });
  },
});
