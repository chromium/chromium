// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_page/settings_subpage.js';
// <if expr="_google_chrome">
import '../internal/icons.html.js';

// </if>

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './geic_subpage.css.js';
import {getHtml} from './geic_subpage.html.js';

const SettingsGeicSubpageElementBase =
    SettingsViewMixinLit(I18nMixinLit(CrLitElement));

export class SettingsGeicSubpageElement extends SettingsGeicSubpageElementBase {
  static get is() {
    return 'settings-geic-subpage';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      defaultTabAccessToggleExpanded_: {type: Boolean},
    };
  }

  protected accessor defaultTabAccessToggleExpanded_: boolean = false;

  protected onDefaultTabAccessExpandClick_() {
    this.defaultTabAccessToggleExpanded_ =
        !this.defaultTabAccessToggleExpanded_;
  }

  protected onDefaultTabAccessToggleExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.defaultTabAccessToggleExpanded_ = e.detail.value;
  }

  // i18nAdvanced is needed to allow for translating strings containing HTML.
  // The sublabel contains <ph> elements which are translated to <a> tags to
  // provide a link in the label.
  //
  // GEiC is only available to enterprise accounts whose data is protected, so
  // the data-protected variant of the glic string always applies.
  protected getDefaultTabAccessSubLabel_(): string {
    return this.i18nAdvanced('glicDefaultTabAccessToggleSublabelDataProtected')
        .toString();
  }

  // TODO(crbug.com/564532074): Update the learn more link.
  protected onDefaultTabAccessToggleSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('glicDefaultTabAccessToggleLearnMoreUrlDataProtected'));
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    const subpage = this.shadowRoot.querySelector('settings-subpage');
    if (subpage) {
      subpage.focusBackButton();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-geic-subpage': SettingsGeicSubpageElement;
  }
}

customElements.define(
    SettingsGeicSubpageElement.is, SettingsGeicSubpageElement);
