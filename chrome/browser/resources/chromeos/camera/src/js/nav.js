// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for navigation of views.
 */
cca.nav = cca.nav || {};

cca.App = cca.App || {};

/**
 * All views stacked in ascending z-order (DOM order) for navigation, and only
 * the topmost visible view is active (clickable/focusable).
 * @type {Array<cca.views.View>}
 */
cca.nav.views_ = [];

/**
 * Index of the current topmost visible view in the stacked views.
 * @type {number}
 */
cca.nav.topmostIndex_ = -1;

/**
 * Sets up navigation for all views, e.g. camera-view, dialog-view, etc.
 * @param {Array<cca.views.View>} views All views in ascending z-order.
 */
cca.nav.setup = function(views) {
  cca.nav.views_ = views;
  // Manage all tabindex usages in cca.nav for navigation.
  document.querySelectorAll('[tabindex]')
      .forEach(
          (element) => cca.util.makeUnfocusableByMouse(
              /** @type {!HTMLElement} */ (element)));
  document.body.addEventListener('keydown', (e) => {
    if (e.key === 'Tab') {
      cca.state.set('tab-navigation', true);
    }
  });
  document.body.addEventListener(
      'pointerdown', () => cca.state.set('tab-navigation', false));
};

/**
 * Finds the view by its id in the stacked views.
 * @param {string} id View identifier.
 * @return {number} Index of the view found; otherwise, -1.
 * @private
 */
cca.nav.findIndex_ = function(id) {
  return cca.nav.views_.findIndex((view) => view.root.id == id);
};

/**
 * Finds the next topmost visible view in the stacked views.
 * @return {number} Index of the view found; otherwise, -1.
 * @private
 */
cca.nav.findNextTopmostIndex_ = function() {
  for (var i = cca.nav.topmostIndex_ - 1; i >= 0; i--) {
    if (cca.nav.isShown_(i)) {
      return i;
    }
  }
  return -1;
};

/**
 * Checks if the view is already shown.
 * @param {number} index Index of the view.
 * @return {boolean} Whether the view is shown or not.
 * @private
 */
cca.nav.isShown_ = function(index) {
  return cca.state.get(cca.nav.views_[index].root.id);
};

/**
 * Shows the view indexed in the stacked views and activates the view only if
 * it becomes the topmost visible view.
 * @param {number} index Index of the view.
 * @return {cca.views.View} View shown.
 * @private
 */
cca.nav.show_ = function(index) {
  var view = cca.nav.views_[index];
  if (!cca.nav.isShown_(index)) {
    cca.state.set(view.root.id, true);
    view.layout();
    if (index > cca.nav.topmostIndex_) {
      if (cca.nav.topmostIndex_ >= 0) {
        cca.nav.inactivate_(cca.nav.topmostIndex_);
      }
      cca.nav.activate_(index);
      cca.nav.topmostIndex_ = index;
    }
  }
  return view;
};

/**
 * Hides the view indexed in the stacked views and inactivate the view if it was
 * the topmost visible view.
 * @param {number} index Index of the view.
 * @private
 */
cca.nav.hide_ = function(index) {
  if (index == cca.nav.topmostIndex_) {
    cca.nav.inactivate_(index);
    var next = cca.nav.findNextTopmostIndex_();
    if (next >= 0) {
      cca.nav.activate_(next);
    }
    cca.nav.topmostIndex_ = next;
  }
  cca.state.set(cca.nav.views_[index].root.id, false);
};

/**
 * Activates the view to be focusable.
 * @param {number} index Index of the view.
 */
cca.nav.activate_ = function(index) {
  // Restore the view's child elements' tabindex and then focus the view.
  var view = cca.nav.views_[index];
  view.root.setAttribute('aria-hidden', 'false');
  view.root.querySelectorAll('[data-tabindex]').forEach((element) => {
    element.setAttribute('tabindex', element.dataset.tabindex);
    element.removeAttribute('data-tabindex');
  });
  view.focus();
};

/**
 * Inactivates the view to be unfocusable.
 * @param {number} index Index of the view.
 */
cca.nav.inactivate_ = function(index) {
  var view = cca.nav.views_[index];
  view.root.setAttribute('aria-hidden', 'true');
  view.root.querySelectorAll('[tabindex]').forEach((element) => {
    element.dataset.tabindex = element.getAttribute('tabindex');
    element.setAttribute('tabindex', '-1');
  });
  document.activeElement.blur();
};

/**
 * Sets the element's tabindex on the view.
 * @param {cca.views.View} view View that the element is on.
 * @param {HTMLElement} element Element whose tabindex to be set.
 * @param {number} tabIndex Tab-index of the element.
 */
cca.nav.setTabIndex = function(view, element, tabIndex) {
  if ((cca.nav.topmostIndex_ >= 0) &&
      (cca.nav.views_[cca.nav.topmostIndex_] == view)) {
    element.tabIndex = tabIndex;
  } else {
    // Remember tabindex by data attribute if the view isn't active.
    element.tabIndex = -1;
    element.dataset.tabindex = tabIndex + '';
  }
};

/**
 * Opens a navigation session of the view; shows the view before entering it and
 * hides the view after leaving it for the ended session.
 * @param {string} id View identifier.
 * @param {...*} args Optional rest parameters for entering the view.
 * @return {!Promise<*>} Promise for the operation or result.
 */
cca.nav.open = function(id, ...args) {
  var index = cca.nav.findIndex_(id);
  return cca.nav.show_(index).enter(...args).finally(() => {
    cca.nav.hide_(index);
  });
};

/**
 * Closes the current navigation session of the view by leaving it.
 * @param {string} id View identifier.
 * @param {*=} condition Optional condition for leaving the view.
 * @return {boolean} Whether successfully leaving the view or not.
 */
cca.nav.close = function(id, condition) {
  var index = cca.nav.findIndex_(id);
  return cca.nav.views_[index].leave(condition);
};

/**
 * Handles key pressed event.
 * @param {Event} event Key press event.
 */
cca.nav.onKeyPressed = function(event) {
  var key = cca.util.getShortcutIdentifier(event);
  var openInspector = (type) => chrome.fileManagerPrivate &&
      chrome.fileManagerPrivate.openInspector(type);
  switch (key) {
    case 'BrowserBack':
      chrome.app.window.current().minimize();
      break;
    case 'Ctrl-Shift-I':
      openInspector('normal');
      break;
    case 'Ctrl-Shift-J':
      openInspector('console');
      break;
    case 'Ctrl-Shift-C':
      openInspector('element');
      break;
    case 'Ctrl-Shift-E':
      (async () => {
        if (!await cca.mojo.DeviceOperator.isSupported()) {
          cca.toast.show('error_msg_expert_mode_not_supported');
          return;
        }
        const newState = !cca.state.get('expert');
        cca.state.set('expert', newState);
        cca.proxy.browserProxy.localStorageSet({expert: newState});
      })();
      break;
    default:
      // Make the topmost visible view handle the pressed key.
      if (cca.nav.topmostIndex_ >= 0 &&
          cca.nav.views_[cca.nav.topmostIndex_].onKeyPressed(key)) {
        event.preventDefault();
      }
  }
};

/**
 * Handles resized window on current all visible views.
 */
cca.nav.onWindowResized = function() {
  // All visible views need being relayout after window is resized.
  for (var i = cca.nav.views_.length - 1; i >= 0; i--) {
    if (cca.nav.isShown_(i)) {
      cca.nav.views_[i].layout();
    }
  }
};
