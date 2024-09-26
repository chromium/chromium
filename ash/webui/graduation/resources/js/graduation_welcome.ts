// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '../strings.m.js';

import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GraduationUiHandler} from '../mojom/graduation_ui.mojom-webui.js';

import {ScreenSwitchEvents} from './graduation_app.js';
import {getTemplate} from './graduation_welcome.html.js';

export class GraduationWelcome extends PolymerElement {
  static get is() {
    return 'graduation-welcome' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The email associated with the current profile.
       */
      profileEmail: String,

      /**
       * The URL of the profile photo of the current user.
       */
      profilePhotoUrl: String,
    };
  }

  private profileEmail: string;
  private profilePhotoUrl: string;

  override ready() {
    super.ready();

    this.getProfileInfo();
  }

  private async getProfileInfo(): Promise<void> {
    const {profileInfo} =
        await GraduationUiHandler.getRemote().getProfileInfo();
    this.profileEmail = profileInfo.email;

    const photoUrl = profileInfo.photoUrl;

    // Extract first frame from image by creating a single frame PNG using
    // url as input if base64 encoded and potentially animated.
    if (photoUrl.startsWith('data:image/png;base64')) {
      this.profilePhotoUrl = convertImageSequenceToPng([photoUrl]);
      return;
    }
    this.profilePhotoUrl = photoUrl;
  }

  private getIconImageSet(iconUrl: string): string {
    return getImage(iconUrl);
  }

  private onGetStartedClicked(): void {
    this.dispatchEvent(new CustomEvent(ScreenSwitchEvents.SHOW_TAKEOUT_UI, {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GraduationWelcome.is]: GraduationWelcome;
  }
}

customElements.define(GraduationWelcome.is, GraduationWelcome);
