// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '../../common/async_util.js';
import {EventHandler} from '../../common/event_handler.js';

/**
 * Main class for FaceGaze.
 */
export class FaceGaze {
  constructor() {
    /**
     * Last seen mouse location (cached from event in onMouseMovedOrDragged_).
     * @private {?{x: number, y: number}}
     */
    this.mouseLocation_ = null;

    /** @private {!EventHandler} */
    this.onMouseMovedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_MOVED,
        event => this.onMouseMovedOrDragged_(event));

    /** @private {!EventHandler} */
    this.onMouseDraggedHandler_ = new EventHandler(
        [], chrome.automation.EventType.MOUSE_DRAGGED,
        event => this.onMouseMovedOrDragged_(event));

    this.init_();
  }

  /**
   * Destructor to remove any listeners.
   */
  onFaceGazeDisabled() {
    this.onMouseMovedHandler_.stop();
    this.onMouseDraggedHandler_.stop();
  }

  /**
   * Initializes FaceGaze.
   * @private
   */
  async init_() {
    chrome.accessibilityPrivate.enableMouseEvents(true);
    const desktop = await AsyncUtil.getDesktop();
    this.onMouseMovedHandler_.setNodes(desktop);
    this.onMouseMovedHandler_.start();
    this.onMouseDraggedHandler_.setNodes(desktop);
    this.onMouseDraggedHandler_.start();

    this.connectToWebCam_();

    // TODO(b/309121742): Listen to magnifier bounds changed so as to update
    // cursor relative position logic when magnifier is running.
  }

  /**
   * Listener for when the mouse position changes.
   * @private
   */
  onMouseMovedOrDragged_(event) {
    this.mouseLocation_ = {x: event.mouseX, y: event.mouseY};
  }

  /** @private */
  connectToWebCam_() {
    // Open camera_stream.html, which will connect to the webcam and pass
    // the stream back to the background page.
    const params = {
      url: chrome.runtime.getURL(
          'accessibility_common/facegaze/camera_stream.html'),
      active: false,
    };
    chrome.tabs.create(params, () => {
      chrome.runtime.onMessage.addListener(message => {
        if (message.type === 'cameraStream') {
          const stream = message.stream;
          // TODO(b/309121742): Pass stream to the Facelandmark API.
        }
      });
    });
  }
}
