// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../dom.js';
import {ViewName} from '../../type.js';

import {LeaveCondition, View} from '../view.js';

/**
 * Base controller of settings view.
 */
export class BaseSettings extends View {
  /**
   * The default focus element when focus on view is reset.
   */
  private readonly defaultFocus: HTMLElement;

  /**
   * The DOM element to be focused when the focus on view is reset by calling
   * |focus()|.
   */
  protected focusElement: HTMLElement;

  /**
   * @param name Name of the view.
   */
  constructor(name: ViewName) {
    super(name, {dismissByEsc: true, dismissByBackgroundClick: true});

    dom.getFrom(this.root, '.menu-header button', HTMLButtonElement)
        .addEventListener('click', () => this.leave());

    this.defaultFocus = dom.getFrom(this.root, '[tabindex]', HTMLElement);

    this.focusElement = this.defaultFocus;
  }

  override focus(): void {
    this.focusElement.focus();
  }

  override leaving(condition: LeaveCondition): boolean {
    this.focusElement = this.defaultFocus;
    return super.leaving(condition);
  }
}
