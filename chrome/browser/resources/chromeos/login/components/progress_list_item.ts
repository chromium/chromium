// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './common_styles/oobe_common_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {getTemplate} from './progress_list_item.html.js';

const ProgressListItemBase = OobeI18nMixin(PolymerElement);

export class ProgressListItem extends ProgressListItemBase {
  static get is() {
    return 'progress-list-item' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
      },

      /* The ID of the localized string to be displayed when item is in
       * the 'active' state. If not specified, the textKey is used instead.
       */
      activeKey: {
        type: String,
        value: '',
      },

      /* The ID of the localized string to be displayed when item is in the
       * 'completed' state. If not specified, the textKey is used instead.
       */
      completedKey: {
        type: String,
        value: '',
      },

      /* Indicates if item is in "active" state. Has higher priority than
       * "completed" below.
       */
      active: {
        type: Boolean,
        value: false,
      },

      /* Indicates if item is in "completed" state. Has lower priority than
       * "active" state above.
       */
      completed: {
        type: Boolean,
        value: false,
      },
    };
  }

  textKey: string;
  activeKey: string;
  completedKey: string;
  active: boolean;
  completed: boolean;

  private hidePending(active: boolean, completed: boolean): boolean {
    return active || completed;
  }

  private hideCompleted(active: boolean, completed: boolean): boolean {
    return active || !completed;
  }

  private fallbackText(locale: string, key: string, fallbackKey: string):
      string {
    if (key === null || key === '') {
      return this.i18nDynamic(locale, fallbackKey);
    }
    return this.i18nDynamic(locale, key);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ProgressListItem.is]: ProgressListItem;
  }
}

customElements.define(ProgressListItem.is, ProgressListItem);
