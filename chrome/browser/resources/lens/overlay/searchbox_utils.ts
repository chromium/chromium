// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SearchboxElement} from '//resources/cr_components/searchbox/searchbox.js';

import type {LensOverlayAppElement} from './lens_overlay_app.js';
import type {LensSidePanelAppElement} from './side_panel/side_panel_app.js';

export function handleEscapeSearchbox(
    element: LensSidePanelAppElement|LensOverlayAppElement,
    searchbox: SearchboxElement, e: CustomEvent) {
  // The searchbox should only be blurred if there is no input. If there is
  // input, cr-searchbox will clear the input and keep focus in the
  // searchbox.
  if (e.detail.emptyInput) {
    searchbox.blur();
  } else {
    // If searchbox input is not empty, the searchbox will still have focus,
    // but the ghost loader should not show since zero suggest is not queried.
    element.suppressGhostLoader = true;
  }
}

export function onSearchboxKeydown(
    element: LensSidePanelAppElement|LensOverlayAppElement,
    searchbox: SearchboxElement) {
  // If a user inputs text into the searchbox, there is a possibility we
  // query zero suggest again and the ghost loader should not be suppressed.
  if (!searchbox.isInputEmpty()) {
    element.suppressGhostLoader = false;
  }
}

export function onEscapeKeyPressed(
    element: LensSidePanelAppElement|LensOverlayAppElement, e: Event) {
  // Prevents the overlay from closing after the searchbox is blurred.
  if (!element.isSearchboxFocused) {
    e.preventDefault();
  }
}
