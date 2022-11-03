// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Replaces the old events view with instructions and links to
 * help users migrate to using net-export and the catapult netlog_viewer.
 */

import {$} from 'chrome://resources/js/util.js';

import {DivView} from './view.js';

// This is defined in index.html, but for all intents and purposes is part
// of this view.
const LOAD_LOG_FILE_DROP_TARGET_ID = 'events-view-drop-target';

/** @type {?EventsView} */
let instance = null;

export class EventsView extends DivView {
  constructor() {
    super(EventsView.MAIN_BOX_ID);

    const dropTarget = $(LOAD_LOG_FILE_DROP_TARGET_ID);
    dropTarget.ondragenter = this.onDrag.bind(this);
    dropTarget.ondragover = this.onDrag.bind(this);
    dropTarget.ondrop = this.onDrop.bind(this);
  }

  /**
   * Prevent default browser behavior when a file is dragged over the page to
   * allow our onDrop() handler to handle the drop.
   */
  onDrag(event) {
    if (event.dataTransfer.types.includes('Files')) {
      event.preventDefault();
    }
  }

  /**
   * If a single file is dropped, redirect to the events tab to show the
   * deprecation message.
   */
  onDrop(event) {
    if (event.dataTransfer.files.length !== 1) {
      return;
    }
    event.preventDefault();

    document.location.hash = 'events';
  }

  static getInstance() {
    return instance || (instance = new EventsView());
  }
}

EventsView.TAB_ID = 'tab-handle-events';
EventsView.TAB_NAME = 'Events';
EventsView.TAB_HASH = '#events';

// ID for special HTML element in events_view.html
EventsView.MAIN_BOX_ID = 'events-view-tab-content';
