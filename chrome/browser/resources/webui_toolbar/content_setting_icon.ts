// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './content_setting_icon.css.js';
import {getHtml} from './content_setting_icon.html.js';
import type {ContentSettingImageState} from './toolbar_ui_api_data_model.mojom-webui.js';
import {ContentSettingImageType} from './toolbar_ui_api_data_model.mojom-webui.js';

export interface ContentSettingIconElement {
  $: {
    button: CrIconButtonElement,
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
      animating: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor state: ContentSettingImageState = {
    type: ContentSettingImageType.kCookies,
    isBlocked: false,
    tooltip: '',
    accessibilityString: '',
    isBubbleVisible: false,
    shouldRunAnimation: false,
    explanatoryString: '',
  };

  protected accessor animating: boolean = false;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('state')) {
      this.animating = this.state.shouldRunAnimation;
    }
  }

  protected onLabelAnimationend_() {
    this.animating = false;
  }

  protected getIconClass_(): string {
    const iconType = this.state.type;
    const blocked = this.state.isBlocked;

    switch (iconType) {
      case ContentSettingImageType.kCookies:
        return blocked ? 'icon-database-off' : 'icon-database';
      case ContentSettingImageType.kImages:
        return blocked ? 'icon-photo-off' : 'icon-photo';
      case ContentSettingImageType.kJavaScript:
        return blocked ? 'icon-code-off' : 'icon-code';
      case ContentSettingImageType.kMixedScript:
        return blocked ? 'icon-not-secure-warning-off' :
                         'icon-not-secure-warning';
      case ContentSettingImageType.kSound:
        return blocked ? 'icon-volume-off' : 'icon-volume-up';
      case ContentSettingImageType.kAds:
        return blocked ? 'icon-ads-off' : 'icon-ads';
      case ContentSettingImageType.kGeolocation:
        return blocked ? 'icon-location-off' : 'icon-location';
      case ContentSettingImageType.kProtocolHandlers:
        return blocked ? 'icon-protocol-handler-off' : 'icon-protocol-handler';
      case ContentSettingImageType.kMidiSysex:
        return blocked ? 'icon-midi-off' : 'icon-midi';
      case ContentSettingImageType.kAutomaticDownloads:
        return blocked ? 'icon-file-download-off' : 'icon-file-download';
      case ContentSettingImageType.kClipboardReadWrite:
        return blocked ? 'icon-content-paste-off' : 'icon-content-paste';
      case ContentSettingImageType.kMediaStream:
        return blocked ? 'icon-videocam-off' : 'icon-videocam';
      case ContentSettingImageType.kNotifications:
        return blocked ? 'icon-notifications-off' : 'icon-notifications';
      case ContentSettingImageType.kSensors:
        return blocked ? 'icon-sensors-off' : 'icon-sensors';
      case ContentSettingImageType.kStorageAccess:
        return blocked ? 'icon-storage-access-off' : 'icon-storage-access';
      case ContentSettingImageType.kPopups:
        return blocked ? 'icon-iframe-off' : 'icon-iframe';
      case ContentSettingImageType.kFramebust:
        return blocked ? 'icon-framebust-off' : 'icon-framebust';
      // <if expr="is_chromeos">
      case ContentSettingImageType.kSmartCard:
        // Indicator shows only when at least one connection is active, hence no
        // need for the off icon.
        return 'icon-smart-card-reader';
      // </if>
      // <if expr="is_win">
      case ContentSettingImageType.kProtectedMediaIdentifier:
        return blocked ? 'icon-sync-saved-locally-off' :
                         'icon-sync-saved-locally';
      // </if>
      default:
        assertNotReachedCase(iconType);
    }
  }

  protected getAriaLabel_(): string {
    return this.state.accessibilityString || this.state.tooltip;
  }

  protected showContentSettingsBubble_() {
    this.browserProxy_.toolbarUIHandler.showContentSettingsBubble(
        this.state.type);
  }

  protected onClick_() {
    this.showContentSettingsBubble_();
  }

  protected onAuxclick_() {
    this.showContentSettingsBubble_();
  }

  protected onContextmenu_() {
    this.showContentSettingsBubble_();
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
