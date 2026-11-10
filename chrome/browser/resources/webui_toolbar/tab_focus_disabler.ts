// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper class that can block tab focus of its children when its
// tabindex is set to -1.
class TabFocusDisabler extends HTMLElement {
  constructor() {
    super();
    // Set delegatesFocus so that setting tabindex to -1 will hide
    // the contents from sequential focus.
    const shadow = this.attachShadow({mode: 'open', delegatesFocus: true});
    shadow.appendChild(document.createElement('slot'));
  }
}
customElements.define('tab-focus-disabler', TabFocusDisabler);
