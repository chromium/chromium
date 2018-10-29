// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Replaces the old events view with instructions and links to
 * help users migrate to using net-export and the catapult netlog_viewer.
 */

var EventsView = (function() {
  'use strict';

  // This is defined in index.html, but for all intents and purposes is part
  // of this view.
  var LOAD_LOG_FILE_DROP_TARGET_ID = 'events-view-drop-target';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   *  @constructor
   */
  function EventsView() {
    assertFirstConstructorCall(EventsView);

    // Call superclass's constructor.
    superClass.call(this, EventsView.MAIN_BOX_ID);

    var dropTarget = $(LOAD_LOG_FILE_DROP_TARGET_ID);
    dropTarget.ondragenter = this.onDrag.bind(this);
    dropTarget.ondragover = this.onDrag.bind(this);
    dropTarget.ondrop = this.onDrop.bind(this);
  }

  EventsView.TAB_ID = 'tab-handle-events';
  EventsView.TAB_NAME = 'Events';
  EventsView.TAB_HASH = '#events';

  // IDs for special HTML elements in dns_view.html
  EventsView.MAIN_BOX_ID = 'events-view-tab-content';

  cr.addSingletonGetter(EventsView);

  EventsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called when something is dragged over the drop target.
     *
     * Returns false to cancel default browser behavior when a single file is
     * being dragged.  When this happens, we may not receive a list of files for
     * security reasons, which is why we allow the |files| array to be empty.
     */
    onDrag: function(event) {
      // NOTE: Use Array.prototype.indexOf here is necessary while WebKit
      // decides which type of data structure dataTransfer.types will be
      // (currently between DOMStringList and Array). These have different APIs
      // so assuming one type or the other was breaking things. See
      // http://crbug.com/115433. TODO(dbeam): Remove when standardized more.
      var indexOf = Array.prototype.indexOf;
      return indexOf.call(event.dataTransfer.types, 'Files') == -1 ||
          event.dataTransfer.files.length > 1;
    },

    /**
     * Called when something is dropped onto the drop target.  If it's a single
     * file, redirect to the events tab to show the depreciation message.
     */
    onDrop: function(event) {
      var indexOf = Array.prototype.indexOf;
      if (indexOf.call(event.dataTransfer.types, 'Files') == -1 ||
          event.dataTransfer.files.length != 1) {
        return;
      }
      event.preventDefault();

      document.location.hash = 'events';
    },
  };

  return EventsView;
})();
