// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import './common_styles/oobe_common_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {getTemplate} from './throbber_notice.html.js';

const ThrobberNoticeBase = OobeI18nMixin(PolymerElement);

export class ThrobberNotice extends ThrobberNoticeBase {
  static get is() {
    return 'throbber-notice' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {textKey: String};
  }

  private textKey: string;

  /**
   * Returns the a11y message to be shown on this throbber,
   * if the textkey is set.
   */
  getAriaLabel(locale: string): string {
    return (!this.textKey) ? '' : this.i18nDynamic(locale, this.textKey);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ThrobberNotice.is]: ThrobberNotice;
  }
}

customElements.define(ThrobberNotice.is, ThrobberNotice);
