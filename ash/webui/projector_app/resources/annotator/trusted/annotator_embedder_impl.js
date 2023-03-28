// Copyright 2021 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotatorBrowserProxyImpl} from './annotator_browser_proxy.js';
import {AnnotatorPageCallbackRouter} from './ash/webui/projector_app/mojom/annotator.mojom-webui.js';
import {AnnotatorTrustedCommFactory} from './trusted_annotator_comm_factory.js';

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


/* @type {UntrustedAnnotatorClient} */
const client = AnnotatorTrustedCommFactory.getPostMessageAPIClient();

/* @type {AnnotatorPageCallbackRouter} */
const annotatorPageRouter =
    AnnotatorBrowserProxyImpl.getInstance().getAnnotatorCallbackRouter();

annotatorPageRouter.undo.addListener(() => {
  try {
    client.undo();
  } catch (error) {
    AnnotatorBrowserProxyImpl.getInstance().onError(
        [AnnotatorToolErrorType.UNDO_ERROR]);
  }
});

annotatorPageRouter.redo.addListener(() => {
  try {
    client.redo();
  } catch (error) {
    AnnotatorBrowserProxyImpl.getInstance().onError(
        [AnnotatorToolErrorType.REDO_ERROR]);
  }
});


annotatorPageRouter.clear.addListener(() => {
  try {
    client.clear();
  } catch (error) {
    AnnotatorBrowserProxyImpl.getInstance().onError(
        [AnnotatorToolErrorType.CLEAR_ERROR]);
  }
});


annotatorPageRouter.setTool.addListener((tool) => {
  try {
    client.setTool(tool);
  } catch (error) {
    AnnotatorBrowserProxyImpl.getInstance().onError(
        [AnnotatorToolErrorType.SET_TOOL_ERROR]);
  }
});