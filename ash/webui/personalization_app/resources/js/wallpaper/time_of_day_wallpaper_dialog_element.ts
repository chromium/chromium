// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Displays a dialog informing the user that selecting a Time of
 * Day wallpaper will override some of settings such as dark/light mode and
 * dynamic color.
 */

import '../../css/cros_button_style.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './time_of_day_wallpaper_dialog_element.html.js';

export class TimeOfDayAcceptEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'time-of-day-wallpaper-dialog-accept';

  constructor() {
    super(
        TimeOfDayAcceptEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: null,
        },
    );
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'time-of-day-wallpaper-dialog': TimeOfDayWallpaperDialog;
  }
}

export interface TimeOfDayWallpaperDialog {
  $: {dialog: CrDialogElement};
}

export class TimeOfDayWallpaperDialog extends PolymerElement {
  static get is() {
    return 'time-of-day-wallpaper-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onClickAccept_() {
    this.dispatchEvent(new TimeOfDayAcceptEvent());
  }

  private onClickClose_() {
    this.$.dialog.cancel();
  }
}

customElements.define(TimeOfDayWallpaperDialog.is, TimeOfDayWallpaperDialog);
