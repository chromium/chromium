// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DragWrapperDelegate} from 'chrome://resources/js/drag_wrapper.js';

import {Service} from './service.js';


declare global {
  interface HTMLElementEventMap {
    'drag-and-drop-load-error':
        CustomEvent<Error|chrome.developerPrivate.LoadError>;
  }
}

export class DragAndDropHandler implements DragWrapperDelegate {
  dragEnabled: boolean;
  private eventTarget_: EventTarget;

  constructor(dragEnabled: boolean, target: EventTarget) {
    this.dragEnabled = dragEnabled;
    this.eventTarget_ = target;
  }

  shouldAcceptDrag(e: DragEvent): boolean {
    // External Extension installation can be disabled globally, e.g. while a
    // different overlay is already showing.
    if (!this.dragEnabled) {
      return false;
    }

    // We can't access filenames during the 'dragenter' event, so we have to
    // wait until 'drop' to decide whether to do something with the file or
    // not.
    // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
    return !!e.dataTransfer!.types &&
        e.dataTransfer!.types.indexOf('Files') > -1;
  }

  doDragEnter() {
    Service.getInstance().notifyDragInstallInProgress();
    this.eventTarget_.dispatchEvent(new CustomEvent('extension-drag-started'));
  }

  doDragLeave() {
    this.fireDragEnded_();
  }

  doDragOver(e: DragEvent) {
    e.preventDefault();
  }

  doDrop(e: DragEvent) {
    this.fireDragEnded_();
    if (e.dataTransfer!.files.length !== 1) {
      return;
    }

    let handled = false;

    // Files lack a check if they're a directory, but we can find out through
    // its item entry.
    const item = e.dataTransfer!.items[0];
    if (item.kind === 'file' && item.webkitGetAsEntry()!.isDirectory) {
      handled = true;
      this.handleDirectoryDrop_();
    } else if (/\.(crx|user\.js|zip)$/i.test(e.dataTransfer!.files[0].name)) {
      // Only process files that look like extensions. Other files should
      // navigate the browser normally.
      handled = true;
      this.handleFileDrop_();
    }

    if (handled) {
      e.preventDefault();
    }
  }

  /**
   * Handles a dropped file.
   */
  private handleFileDrop_() {
    Service.getInstance().installDroppedFile();
  }

  /**
   * Handles a dropped directory.
   */
  private handleDirectoryDrop_() {
    Service.getInstance().loadUnpackedFromDrag().catch(loadError => {
      this.eventTarget_.dispatchEvent(
          new CustomEvent('drag-and-drop-load-error', {detail: loadError}));
    });
  }

  private fireDragEnded_() {
    this.eventTarget_.dispatchEvent(new CustomEvent('extension-drag-ended'));
  }
}
