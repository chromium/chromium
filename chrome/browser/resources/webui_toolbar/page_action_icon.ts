// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {IconTable} from '/shared/icon_table.js';
import type {PageActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PageActionId, PageActionTrigger} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './page_action_icon.css.js';
import {getHtml} from './page_action_icon.html.js';
import {getClickSourceType} from './toolbar_button.js';

export interface PageActionIconElement {
  $: {
    button: CrIconButtonElement,
  };
}

export class PageActionIconElement extends CrLitElement {
  static get is() {
    return 'page-action-icon';
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

      // Draw a focus ring as if focused.
      forceFocusRing: {type: Boolean},
    };
  }

  accessor state: PageActionState = {
    pageActionId: PageActionId.kActionAiMode,
    accessibleName: '',
    tooltipText: '',
    icon: {handleId: 0n},
  };

  accessor forceFocusRing: boolean = false;

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('forceFocusRing')) {
      this.classList.toggle('force-focus-ring', this.forceFocusRing);
    }
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private iconTable_: IconTable = IconTable.getInstance();

  protected getIronIcon_(): string|undefined {
    return this.iconTable_.getIconName(this.state.icon);
  }

  protected getIconStyle_(): string|undefined {
    const providedIconUrl = this.iconTable_.getIconMaskUrl(this.state.icon);
    const providedIconColor = this.iconTable_.getIconColor(this.state.icon);
    let style = '';

    if (providedIconUrl) {
      style += `--cr-icon-image: url(${providedIconUrl});`;
    }
    if (providedIconColor) {
      style += `--cr-icon-button-fill-color: ${providedIconColor};`;
    }
    return style.length > 0 ? style : undefined;
  }

  protected getAriaLabel_(): string {
    return this.state.accessibleName;
  }

  protected onClick_(e: Event) {
    this.browserProxy_.toolbarUIHandler.onPageActionClick(
        this.state.pageActionId, this.getPageActionTrigger_(e));
  }

  private getPageActionTrigger_(e: Event): PageActionTrigger {
    const sourceType = getClickSourceType(e);
    switch (sourceType) {
      case MenuSourceType.kTouch:
        return PageActionTrigger.kGesture;
      case MenuSourceType.kKeyboard:
        return PageActionTrigger.kKeyboard;
      case MenuSourceType.kMouse:
        return PageActionTrigger.kMouse;
      default:
        throw new Error('Unknown sourceType ' + sourceType);
    }
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
    'page-action-icon': PageActionIconElement;
  }
}

customElements.define(PageActionIconElement.is, PageActionIconElement);
