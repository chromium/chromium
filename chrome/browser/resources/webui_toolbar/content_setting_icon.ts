// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';

import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {ContentSettingImageState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {ContentSettingImageType} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './content_setting_icon.css.js';
import {getHtml} from './content_setting_icon.html.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface ContentSettingIconElement {
  $: {
    chip: ToolbarChipButtonElement,
    label: HTMLElement,
  };
}

export class ContentSettingIconElement extends CrLitElement {
  static get is() {
    return 'content-setting-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},
    };
  }

  accessor state: ContentSettingImageState = {
    type: ContentSettingImageType.kCookies,
    isBlocked: false,
    tooltip: '',
    accessibilityString: '',
    isBubbleVisible: false,
    explanatoryString: '',
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected getIconUrl_(): string {
    const iconType = this.state.type;
    const blocked = this.state.isBlocked;
    let iconName = '';

    switch (iconType) {
      case ContentSettingImageType.kCookies:
        iconName = blocked ? 'database_off' : 'database';
        break;
      case ContentSettingImageType.kImages:
        iconName =
            blocked ? 'photo_off_chrome_refresh' : 'photo_chrome_refresh';
        break;
      case ContentSettingImageType.kJavaScript:
        iconName = blocked ? 'code_off_chrome_refresh' : 'code_chrome_refresh';
        break;
      case ContentSettingImageType.kMixedScript:
        iconName = blocked ? 'not_secure_warning_off_chrome_refresh' :
                             'not_secure_warning_chrome_refresh';
        break;
      case ContentSettingImageType.kSound:
        iconName =
            blocked ? 'volume_off_chrome_refresh' : 'volume_up_chrome_refresh';
        break;
      case ContentSettingImageType.kAds:
        iconName = blocked ? 'ads_off_chrome_refresh' : 'ads_chrome_refresh';
        break;
      case ContentSettingImageType.kGeolocation:
        iconName = blocked ? 'location_off_chrome_refresh' :
                             'location_on_chrome_refresh';
        break;
      case ContentSettingImageType.kProtocolHandlers:
        iconName = blocked ? 'protocol_handler_off_chrome_refresh' :
                             'protocol_handler_chrome_refresh';
        break;
      case ContentSettingImageType.kMidiSysex:
        iconName = blocked ? 'midi_off_chrome_refresh' : 'midi_chrome_refresh';
        break;
      case ContentSettingImageType.kAutomaticDownloads:
        iconName = blocked ? 'file_download_off_chrome_refresh' :
                             'file_download_chrome_refresh';
        break;
      case ContentSettingImageType.kClipboardReadWrite:
        iconName = blocked ? 'content_paste_off' : 'content_paste';
        break;
      case ContentSettingImageType.kMediaStream:
        iconName =
            blocked ? 'videocam_off_chrome_refresh' : 'videocam_chrome_refresh';
        break;
      case ContentSettingImageType.kNotifications:
        iconName = blocked ? 'notifications_off_chrome_refresh' :
                             'notifications_chrome_refresh';
        break;
      case ContentSettingImageType.kSensors:
        iconName =
            blocked ? 'sensors_off_chrome_refresh' : 'sensors_chrome_refresh';
        break;
      case ContentSettingImageType.kStorageAccess:
        iconName = blocked ? 'storage_access_off' : 'storage_access';
        break;
      case ContentSettingImageType.kPopups:
        iconName = blocked ? 'iframe_off' : 'iframe';
        break;
      case ContentSettingImageType.kFramebust:
        iconName = blocked ? 'open_in_new_off_chrome_refresh' :
                             'open_in_new_chrome_refresh';
        break;
      // <if expr="is_chromeos">
      case ContentSettingImageType.kSmartCard:
        // Indicator shows only when at least one connection is active, hence no
        // need for the off icon.
        iconName = 'smart_card_reader';
        break;
      // </if>
      // <if expr="is_win">
      case ContentSettingImageType.kProtectedMediaIdentifier:
        iconName = blocked ? 'sync_saved_locally_off' : 'sync_saved_locally';
        break;
      // </if>
      default:
        assertNotReachedCase(iconType);
    }
    return `url('shared/rhs_icons/${iconName}.svg')`;
  }

  protected getAriaLabel_(): string {
    return this.state.accessibilityString || this.state.tooltip;
  }

  protected showContentSettingsBubble_(e: PointerEvent) {
    // Keyboard synthetic clicks generate PointerEvents with an empty
    // pointerType in WebUI, whereas natural pointer clicks have a valid
    // pointerType (e.g., 'mouse', 'touch', 'pen').
    const isPointerInteraction = !!e.pointerType;
    this.browserProxy_.toolbarUIHandler.showContentSettingsBubble(
        this.state.type, isPointerInteraction);
  }

  protected onClick_(e: PointerEvent) {
    this.showContentSettingsBubble_(e);
  }

  protected onAuxclick_(e: PointerEvent) {
    // Handles both middle and right clicks.
    this.showContentSettingsBubble_(e);
  }

  protected onContextmenu_(e: PointerEvent) {
    // Prevent the default browser context menu on right click. The right click
    // action is natively handled as opening the bubble, which we process in
    // onAuxclick_ instead to avoid double-triggering.
    e.preventDefault();
  }

  protected onPointerdown_() {
    this.browserProxy_.toolbarUIHandler.onContentSettingImagePointerDown(
        this.state.type);
  }

  protected onPointerenter_() {
    this.fire('chip-pointerenter');
  }

  protected onPointerleave_() {
    this.fire('chip-pointerleave');
  }

  protected onPointercancel_() {
    this.fire('chip-pointercancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-setting-icon': ContentSettingIconElement;
  }
}

customElements.define(ContentSettingIconElement.is, ContentSettingIconElement);
