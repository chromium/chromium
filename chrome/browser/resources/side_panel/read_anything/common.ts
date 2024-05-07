// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

// Determined by experimentation - can be adjusted to fine tune for different
// platforms.
export const minOverflowLengthToScroll = 75;
export const defaultFontName: string = 'sans-serif';

// Defines the valid font names that can be passed to front-end and maps
// them to a corresponding class style in app.html. Must stay in-sync with
// the names set in read_anything_model.cc.
const validFontNames: Array<{name: string, css: string}> = [
  {name: 'Poppins', css: 'Poppins'},
  {name: 'Sans-serif', css: 'sans-serif'},
  {name: 'Serif', css: 'serif'},
  {name: 'Comic Neue', css: '"Comic Neue"'},
  {name: 'Lexend Deca', css: '"Lexend Deca"'},
  {name: 'EB Garamond', css: '"EB Garamond"'},
  {name: 'STIX Two Text', css: '"STIX Two Text"'},
  {name: 'Andika', css: 'Andika'},
];

const ACTIVE_CSS_CLASS = 'active';

// Validate that the given font name is a valid choice, or use the default.
export function validatedFontName(fontName: string): string {
  const validFontName =
      validFontNames.find((f: {name: string}) => f.name === fontName);
  return validFontName ? validFontName.css : defaultFontName;
}

export function openMenu(
    menuToOpen: CrActionMenuElement, target: HTMLElement,
    showAtConfig?: {minX: number, maxX: number}) {
  // The button should stay active while the menu is open and deactivate when
  // the menu closes.
  menuToOpen.addEventListener('close', () => {
    target.classList.remove(ACTIVE_CSS_CLASS);
  });
  target.classList.add(ACTIVE_CSS_CLASS);

  // TODO(b/337058857): We shouldn't need to wrap this twice in
  // requestAnimationFrame in order to get an accessible label to be read by
  // ChromeVox. We should investigate more in what's going on with
  // cr-action-menu to find a better long-term solution. This is sufficient
  // for now.
  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      const minY = target.getBoundingClientRect().bottom;
      menuToOpen.showAt(
          target,
          Object.assign(
              {
                minY: minY,
                anchorAlignmentX: AnchorAlignment.AFTER_START,
                anchorAlignmentY: AnchorAlignment.AFTER_END,
                noOffset: true,
              },
              showAtConfig));
    });
  });
}
