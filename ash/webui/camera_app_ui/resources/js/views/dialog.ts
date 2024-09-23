// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertString} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import {getI18nMessage} from '../models/load_time_data.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {ViewName} from '../type.js';

import {DialogEnterOptions, View} from './view.js';

interface ButtonEvent {
  onNegativeButtonClicked?: () => void;
  onPositiveButtonClicked?: () => void;
}

/**
 * Creates the Dialog view controller.
 */
export class Dialog extends View {
  private readonly positiveButton: HTMLButtonElement;

  private readonly negativeButton: HTMLButtonElement|null;

  private readonly messageHolder: HTMLElement;

  private readonly titleHolder: HTMLDivElement|null;

  protected readonly descHolder: HTMLDivElement|null;

  constructor(
      name: ViewName,
      {onPositiveButtonClicked, onNegativeButtonClicked}: ButtonEvent = {}) {
    super(
        name,
        {dismissByEsc: true, defaultFocusSelector: '.dialog-positive-button'});

    this.positiveButton =
        dom.getFrom(this.root, '.dialog-positive-button', HTMLButtonElement);
    this.negativeButton = dom.getFromIfExists(
        this.root, '.dialog-negative-button', HTMLButtonElement);
    this.messageHolder =
        dom.getFrom(this.root, '.dialog-msg-holder', HTMLElement);
    this.titleHolder =
        dom.getFromIfExists(this.root, '.dialog-title', HTMLDivElement);
    this.descHolder =
        dom.getFromIfExists(this.root, '.dialog-description', HTMLDivElement);

    this.positiveButton.addEventListener('click', () => {
      onPositiveButtonClicked?.();
      this.leave({kind: 'CLOSED', val: true});
    });
    this.negativeButton?.addEventListener('click', () => {
      onNegativeButtonClicked?.();
      this.leave();
    });
  }

  override entering(
      {cancellable, description, message, title}: DialogEnterOptions = {}):
      void {
    // Update dialog text
    if (message !== undefined) {
      this.messageHolder.textContent = assertString(message);
    }
    // Update title and description, and update i18n-text for testing purpose.
    if (title !== undefined && this.titleHolder !== null) {
      this.titleHolder.textContent = getI18nMessage(title);
      this.titleHolder.setAttribute('i18n-text', title);
    }
    if (description !== undefined && this.descHolder !== null) {
      this.descHolder.textContent = getI18nMessage(description);
      this.descHolder.setAttribute('i18n-text', description);
    }

    // Only change visibility when explicitly define boolean value.
    if (this.negativeButton !== null && cancellable !== undefined) {
      this.negativeButton.hidden = !cancellable;
    }
  }
}

const HELP_PAGE_URL =
    'https://support.google.com/chromebook/?p=camera_usage_on_chromebook';
export class SuperResIntroDialog extends Dialog {
  constructor() {
    const learnMoreAction = () => {
      ChromeHelper.getInstance().openUrlInBrowser(HELP_PAGE_URL);
      this.leave();
    };
    super(
        ViewName.SUPER_RES_INTRO_DIALOG,
        {onNegativeButtonClicked: learnMoreAction});
  }

  override entering(): void {
    // Replace the description placeholder with PTZ button icon.
    assert(this.descHolder !== null);

    const ptzIconPlaceholder = '<PTZ ICON>';
    const desc = getI18nMessage(
        I18nString.SUPER_RES_INTRO_DIALOG_DESC, ptzIconPlaceholder);
    const replacePosition = desc.indexOf(ptzIconPlaceholder);

    const textNode = document.createTextNode(desc);
    const textAfterIcon = textNode.splitText(replacePosition)
                              .splitText(ptzIconPlaceholder.length);

    const icon = document.createElement('svg-wrapper');
    icon.name = 'camera_button_ptz_panel.svg';

    const iconWrapper = document.createElement('span');
    iconWrapper.classList.add('ptz-icon');
    iconWrapper.setAttribute(
        'aria-label', getI18nMessage(I18nString.OPEN_PTZ_PANEL_BUTTON));
    iconWrapper.appendChild(icon);

    this.descHolder.append(textNode, iconWrapper, textAfterIcon);
  }
}
