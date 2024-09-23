// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import './privacy_indicator_app_manager.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {pageHandler} from './page_handler.js';
import {getTemplate} from './status_area_internals.html.js';

/**
 * @fileoverview
 * 'status-area-internals' defines the UI for the "ChromeOS Status Area
 * Internals" test page.
 */

function getBatteryIconEnum(icon: string): number {
  switch (icon) {
    case 'x-icon':
      return 1;
    case 'unreliable-icon':
      return 2;
    case 'bolt-icon':
      return 3;
    case 'battery-saver-plus-icon':
      return 4;
    default:
      return 0;
  }
}

export class StatusAreaInternalsElement extends PolymerElement {
  static get is() {
    return 'status-area-internals';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  onImeToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.toggleImeTray(toggled);
  }

  onPaletteToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.togglePaletteTray(toggled);
  }

  onLogoutToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.toggleLogoutTray(toggled);
  }

  onVirtualKeyboardToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.toggleVirtualKeyboardTray(toggled);
  }

  onDictationToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.toggleDictationTray(toggled);
  }

  onVideoConferenceToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.toggleVideoConferenceTray(toggled);
  }

  onAnnotatorToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.toggleAnnotationTray(toggled);
  }

  onChildUserToggled(e: CustomEvent<boolean>) {
    e.stopPropagation();

    const toggled = e.detail;
    pageHandler.setIsInUserChildSession(toggled);
  }

  onHmrConsentStatusReset(e: CustomEvent<boolean>) {
    e.stopPropagation();

    pageHandler.resetHmrConsentStatus();
  }

  onBatteryIconChanged(e: CustomEvent<string>) {
    e.stopPropagation();

    const selectedIcon = (e.target as HTMLInputElement).value;
    pageHandler.setBatteryIcon(getBatteryIconEnum(selectedIcon));
  }

  onBatteryPercentChanged(e: CustomEvent<number>) {
    e.stopPropagation();

    const target = e.target as HTMLInputElement;
    const value = target.value;
    pageHandler.setBatteryPercent(parseInt(value, 10));
  }
}

customElements.define(
    StatusAreaInternalsElement.is, StatusAreaInternalsElement);
