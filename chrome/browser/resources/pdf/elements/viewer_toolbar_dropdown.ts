// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './viewer_toolbar_dropdown.html.js';

/**
 * Size of additional padding in the inner scrollable section of the dropdown.
 */
const DROPDOWN_INNER_PADDING: number = 12;

/** Size of vertical padding on the outer #dropdown element. */
const DROPDOWN_OUTER_PADDING: number = 2;

/** Minimum height of toolbar dropdowns (px). */
const MIN_DROPDOWN_HEIGHT: number = 200;

export interface ViewerToolbarDropdownElement {
  $: {
    button: CrIconButtonElement,
    dropdown: HTMLElement,
    'scroll-container': HTMLElement,
  };
}

export class ViewerToolbarDropdownElement extends PolymerElement {
  static get is() {
    return 'viewer-toolbar-dropdown';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Icon to display when the dropdown is closed. */
      closedIcon: String,

      /** Whether the dropdown should be centered or right aligned. */
      dropdownCentered: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /** True if the dropdown is currently open. */
      dropdownOpen: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * String to be displayed at the top of the dropdown and for the tooltip
       * of the button.
       */
      header: String,

      /** Whether to hide the header at the top of the dropdown. */
      hideHeader: {
        type: Boolean,
        value: false,
      },

      /** Lowest vertical point that the dropdown should occupy (px). */
      lowerBound: {
        type: Number,
        observer: 'lowerBoundChanged_',
      },

      /** Whether the dropdown must be selected before opening. */
      openAfterSelect: {
        type: Boolean,
        value: false,
      },

      /** Icon to display when the dropdown is open. */
      openIcon: String,

      /** Whether the dropdown is marked as selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /** Toolbar icon currently being displayed. */
      dropdownIcon_: {
        type: String,
        computed: 'computeIcon_(dropdownOpen, closedIcon, openIcon)',
      },
    };
  }

  closedIcon: string;
  dropdownCentered: boolean;
  dropdownOpen: boolean;
  header: string;
  hideHeader: boolean;
  lowerBound: number;
  openAfterSelect: boolean;
  openIcon: string;
  selected: boolean;

  /** Current animation being played, or null if there is none. */
  private animation_: Animation|null = null;

  private dropdownIcon_: string;

  /**
   * True if the max-height CSS property for the dropdown scroll container is
   * valid. If false, the height will be updated the next time the dropdown is
   * visible.
   */
  private maxHeightValid_: boolean = false;

  /** @return Current icon for the dropdown. */
  private computeIcon_(
      dropdownOpen: boolean, closedIcon: string, openIcon: string): string {
    return dropdownOpen ? openIcon : closedIcon;
  }

  private lowerBoundChanged_() {
    this.maxHeightValid_ = false;
    if (this.dropdownOpen) {
      this.updateMaxHeight();
    }
  }

  toggleDropdown() {
    if (!this.dropdownOpen && this.openAfterSelect && !this.selected) {
      // The dropdown has `openAfterSelect` set, but is not yet selected.
      return;
    }
    this.dropdownOpen = !this.dropdownOpen;
    if (this.dropdownOpen) {
      this.$.dropdown.style.display = 'block';
      if (!this.maxHeightValid_) {
        this.updateMaxHeight();
      }

      const listener = (e: PointerEvent) => {
        if (e.composedPath().includes(this)) {
          return;
        }
        if (this.dropdownOpen) {
          this.toggleDropdown();
          this.blur();
        }
        // Clean up the handler. The dropdown may already be closed.
        window.removeEventListener('pointerdown', listener);
      };
      window.addEventListener('pointerdown', listener);
    }

    this.playAnimation_(this.dropdownOpen);
  }

  updateMaxHeight() {
    const scrollContainer = this.$['scroll-container'];
    let height = this.lowerBound - scrollContainer.getBoundingClientRect().top -
        DROPDOWN_INNER_PADDING;
    height = Math.max(height, MIN_DROPDOWN_HEIGHT);
    scrollContainer.style.maxHeight = height + 'px';
    this.maxHeightValid_ = true;
  }

  /**
   * Start an animation on the dropdown.
   * @param isEntry True to play entry animation, false to play exit.
   */
  private playAnimation_(isEntry: boolean) {
    this.animation_ = isEntry ? this.animateEntry_() : this.animateExit_();
    this.animation_.onfinish = () => {
      this.animation_ = null;
      if (!this.dropdownOpen) {
        this.$.dropdown.style.display = 'none';
      }
    };
  }

  private animateEntry_(): Animation {
    let maxHeight =
        this.$.dropdown.getBoundingClientRect().height - DROPDOWN_OUTER_PADDING;

    if (maxHeight < 0) {
      maxHeight = 0;
    }

    this.$.dropdown.animate([{opacity: 0}, {opacity: 1}], {
      duration: 150,
      easing: 'cubic-bezier(0, 0, 0.2, 1)',
    });
    return this.$.dropdown.animate(
        [
          {height: '20px', transform: 'translateY(-10px)'},
          {height: maxHeight + 'px', transform: 'translateY(0)'},
        ],
        {
          duration: 250,
          easing: 'cubic-bezier(0, 0, 0.2, 1)',
        });
  }

  private animateExit_(): Animation {
    return this.$.dropdown.animate(
        [
          {transform: 'translateY(0)', opacity: 1},
          {transform: 'translateY(-5px)', opacity: 0},
        ],
        {
          duration: 100,
          easing: 'cubic-bezier(0.4, 0, 1, 1)',
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-toolbar-dropdown': ViewerToolbarDropdownElement;
  }
}

customElements.define(
    ViewerToolbarDropdownElement.is, ViewerToolbarDropdownElement);
