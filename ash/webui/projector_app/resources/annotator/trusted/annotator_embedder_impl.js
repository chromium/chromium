// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AnnotatorBrowserProxyImpl} from './annotator_browser_proxy.js';
import {AnnotatorTrustedCommFactory, UntrustedAnnotatorClient} from './trusted_annotator_comm_factory.js';

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
        AnnotatorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.UNDO_ERROR]);
      }
    });

    this.addWebUIListener('redo', () => {
      try {
        client.redo();
      } catch (error) {
        AnnotatorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.REDO_ERROR]);
      }
    });

    this.addWebUIListener('clear', () => {
      try {
        client.clear();
      } catch (error) {
        AnnotatorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.CLEAR_ERROR]);
      }
    });

    this.addWebUIListener('setTool', async (tool) => {
      try {
        client.setTool(tool);
      } catch (error) {
        AnnotatorBrowserProxyImpl.getInstance().onError(
            [AnnotatorToolErrorType.SET_TOOL_ERROR]);
      }
    });
  },
});
