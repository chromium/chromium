// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around a button that toggles
 * between an opened (expanded) and closed state.
 *
 * Forked from
 * ui/webui/resources/cr_elements/cr_expand_button/cr_expand_button.ts
 */
import '../cr_actionable_row_style.css.js';
import '../cr_icon_button/cr_icon_button.js';
import '../cr_shared_vars.css.js';
import '../icons.html.js';

import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';

import {getTemplate} from './cr_expand_button.html.js';

export interface CrExpandButtonElement {
  $: {
    icon: CrIconButtonElement,
  };
}

export class CrExpandButtonElement extends PolymerElement {
  static get is() {
    return 'cr-expand-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If true, the button is in the expanded state and will show the icon
       * specified in the `collapseIcon` property. If false, the button shows
       * the icon specified in the `expandIcon` property.
       */
      expanded: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onExpandedChange_',
      },

      /**
       * If true, the button will be disabled and grayed out.
       */
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** A11y text descriptor for this control. */
      ariaLabel: {
        type: String,
        observer: 'onAriaLabelChange_',
      },

      tabIndex: {
        type: Number,
        value: 0,
      },

      expandIcon: {
        type: String,
        value: 'cr:expand-more',
        observer: 'onIconChange_',
      },

      collapseIcon: {
        type: String,
        value: 'cr:expand-less',
        observer: 'onIconChange_',
      },

      expandTitle: String,
      collapseTitle: String,

      tooltipText_: {
        type: String,
        computed: 'computeTooltipText_(expandTitle, collapseTitle, expanded)',
        observer: 'onTooltipTextChange_',
      },
    };
  }

  expanded: boolean;
  disabled: boolean;
  expandIcon: string;
  collapseIcon: string;
  expandTitle: string;
  collapseTitle: string;
  private tooltipText_: string;

  static get observers() {
    return ['updateAriaExpanded_(disabled, expanded)'];
  }

  override ready() {
    super.ready();
    this.addEventListener('click', this.toggleExpand_);
  }

  private computeTooltipText_(): string {
    return this.expanded ? this.collapseTitle : this.expandTitle;
  }

  private onTooltipTextChange_() {
    this.title = this.tooltipText_;
  }

  override focus() {
    this.$.icon.focus();
  }

  private onAriaLabelChange_() {
    if (this.ariaLabel) {
      this.$.icon.removeAttribute('aria-labelledby');
      this.$.icon.setAttribute('aria-label', this.ariaLabel);
    } else {
      this.$.icon.removeAttribute('aria-label');
      this.$.icon.setAttribute('aria-labelledby', 'label');
    }
  }

  private onExpandedChange_() {
    this.updateIcon_();
  }

  private onIconChange_() {
    this.updateIcon_();
  }

  private updateIcon_() {
    this.$.icon.ironIcon = this.expanded ? this.collapseIcon : this.expandIcon;
  }

  private toggleExpand_(event: Event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    event.stopPropagation();
    event.preventDefault();

    this.scrollIntoViewIfNeeded();
    this.expanded = !this.expanded;
    focusWithoutInk(this.$.icon);
  }

  private updateAriaExpanded_() {
    if (this.disabled) {
      this.$.icon.removeAttribute('aria-expanded');
    } else {
      this.$.icon.setAttribute(
          'aria-expanded', this.expanded ? 'true' : 'false');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-expand-button': CrExpandButtonElement;
  }
}

customElements.define(CrExpandButtonElement.is, CrExpandButtonElement);
