// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {toastDurationMs} from './common.js';
import {getCss} from './language_toast.css.js';
import {getHtml} from './language_toast.html.js';
import {NotificationType} from './voice_language_util.js';
import type {VoiceNotificationListener} from './voice_notification_manager.js';

export interface LanguageToastElement {
  $: {
    toast: CrToastElement,
  };
}

const LanguageToastElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class LanguageToastElement extends LanguageToastElementBase implements
    VoiceNotificationListener {
  static get is() {
    return 'language-toast';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      toastTitle_: {type: String},
      toastMessage_: {type: String},
      showErrors: {type: Boolean},
      availableVoices: {type: Array},
    };
  }

  protected notifications_: Map<string, NotificationType> = new Map();
  protected toastDuration_: number = toastDurationMs;
  protected toastTitle_: string;
  protected toastMessage_: string;

  // We don't want to show error toasts when the language menu is open, so this
  // is set from the parent via one-way binding.
  showErrors: boolean;
  availableVoices: SpeechSynthesisVoice[];

  notify(language: string, type: NotificationType) {
    const previousNotification = this.notifications_.get(language);
    this.notifications_.set(language, type);
    switch (type) {
      case NotificationType.NO_INTERNET:
        // Only show a toast if there are no voices at all.
        if (!this.showErrors || this.availableVoices.length) {
          return;
        }
        this.setErrorTitle_('cantUseReadAloud');
        break;
      case NotificationType.NO_SPACE:
        // Only show a toast if there are no voices at all.
        if (!this.showErrors || this.availableVoices.length) {
          return;
        }
        this.setErrorTitle_('allocationErrorNoVoices');
        break;
      case NotificationType.NO_SPACE_HQ:
        if (!this.showErrors) {
          return;
        }
        this.setErrorTitle_('allocationErrorHighQuality');
        break;
      case NotificationType.DOWNLOADED:
        // TODO(crbug.com/325962407): replace toast with system notification.
        // <if expr="chromeos_ash">
        // Only show a notification for a newly completed download.
        if (previousNotification === NotificationType.DOWNLOADING) {
          const lang =
              chrome.readingMode.getDisplayNameForLocale(language, language) ||
              language;
          this.toastTitle_ =
              loadTimeData.getStringF('readingModeVoiceDownloadedTitle', lang);
          this.toastMessage_ =
              loadTimeData.getString('readingModeVoiceDownloadedMessage');
          break;
        }
        // </if>
        return;
      default:
        return;
    }

    this.show_();
  }

  private setErrorTitle_(message: string) {
    this.toastTitle_ = loadTimeData.getString(message);
    this.toastMessage_ = '';
  }

  private show_() {
    const toast = this.$.toast;
    if (toast.open) {
      toast.hide();
    }
    toast.show();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'language-toast': LanguageToastElement;
  }
}

customElements.define(LanguageToastElement.is, LanguageToastElement);
