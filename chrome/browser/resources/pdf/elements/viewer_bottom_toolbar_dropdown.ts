// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PluginController, PluginControllerEventType} from '../controller.js';

import {getCss} from './viewer_bottom_toolbar_dropdown.css.js';
import {getHtml} from './viewer_bottom_toolbar_dropdown.html.js';

export class ViewerBottomToolbarDropdownElement extends CrLitElement {
  static get is() {
    return 'viewer-bottom-toolbar-dropdown';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      buttonIcon: {type: String},
      showDropdown_: {type: Boolean},
    };
  }

  buttonIcon: string = '';
  protected showDropdown_: boolean = false;

  private pluginController_: PluginController = PluginController.getInstance();
  private tracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.tracker_.add(
        this.pluginController_.getEventTarget(),
        PluginControllerEventType.FINISH_INK_STROKE,
        this.handleFinishInkStroke_.bind(this));
  }

  override disconnectedCallback() {
    this.tracker_.removeAll();
    super.disconnectedCallback();
  }

  protected toggleDropdown_(): void {
    this.showDropdown_ = !this.showDropdown_;

    if (this.showDropdown_) {
      this.tracker_.add(window, 'click', this.handleClick_.bind(this));
    } else {
      this.tracker_.remove(window, 'click');
    }
  }


  // Exit out of the dropdown when the user clicks elsewhere.
  private handleClick_(e: Event) {
    if (!this.showDropdown_ || e.composedPath().includes(this)) {
      return;
    }

    this.toggleDropdown_();
  }

  // TODO(crbug.com/369653190): Ideally, the dropdown should be toggled when the
  // stroke starts, not when the stroke finishes. Exit out of the dropdown when
  // the user draws an ink stroke.
  private handleFinishInkStroke_() {
    if (this.showDropdown_) {
      this.toggleDropdown_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-bottom-toolbar-dropdown': ViewerBottomToolbarDropdownElement;
  }
}

customElements.define(
    ViewerBottomToolbarDropdownElement.is, ViewerBottomToolbarDropdownElement);
