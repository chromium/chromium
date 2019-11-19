// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /**
   * Reverse the child elements of any found button strips if they haven't
   * already been reversed. This is necessary because WebKit does not alter the
   * tab order for elements that are visually reversed using flex-direction:
   * reverse, and the button order is reversed for views. See
   * http://webk.it/62664 for more information.
   * @param {Node=} opt_root Starting point for button strips to reverse.
   */
  function reverseButtonStrips(opt_root) {
    if (!(cr.isWindows || cr.isChromeOS)) {
      // Only reverse on platforms that need it (differ from the HTML order).
      return;
    }

    const root = opt_root || document;
    const buttonStrips = root.querySelectorAll('.button-strip:not([reversed])');
    for (let j = 0; j < buttonStrips.length; j++) {
      const buttonStrip = buttonStrips[j];

      const childNodes = buttonStrip.childNodes;
      for (let i = childNodes.length - 1; i >= 0; i--) {
        buttonStrip.appendChild(childNodes[i]);
      }

      buttonStrip.setAttribute('reversed', '');
    }
  }

  /**
   * Finds a good place to set initial focus. Generally called when UI is shown.
   * @param {!Element} root Where to start looking for focusable controls.
   */
  function setInitialFocus(root) {
    // Do not change focus if any element in |root| is already focused.
    if (root.contains(document.activeElement)) {
      return;
    }

    const elements =
        root.querySelectorAll('input, list, select, textarea, button');
    for (let i = 0; i < elements.length; i++) {
      const element = elements[i];
      element.focus();
      // .focus() isn't guaranteed to work. Continue until it does.
      if (document.activeElement == element) {
        return;
      }
    }
  }

  return {
    reverseButtonStrips: reverseButtonStrips,
    setInitialFocus: setInitialFocus,
  };
});
