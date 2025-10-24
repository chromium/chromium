// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';

const RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU =
    'reloadButtonTooltipReloadWithMenu';
const RELOAD_BUTTON_TOOLTIP_RELOAD = 'reloadButtonTooltipReload';
const RELOAD_BUTTON_TOOLTIP_STOP = 'reloadButtonTooltipStop';

export class ReloadButtonAppElement extends CrLitElement {
  constructor() {
    super();
    const callbackRouter = BrowserProxyImpl.getInstance().callbackRouter;
    callbackRouter.setReloadButtonState.addListener(
        (isLoading: boolean, isMenuEnabled: boolean) => {
          this.isLoading_ = isLoading;
          this.tooltip_ = loadTimeData.getString(
              isLoading ?
                  RELOAD_BUTTON_TOOLTIP_STOP :
                  (isMenuEnabled ? RELOAD_BUTTON_TOOLTIP_RELOAD_WITH_MENU :
                                   RELOAD_BUTTON_TOOLTIP_RELOAD));
        });
    ColorChangeUpdater.forDocument().start();
  }

  static get is() {
    return 'reload-button-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isLoading_: {state: true, type: Boolean},
      tooltip_: {state: true, type: String},
    };
  }

  protected accessor isLoading_: boolean = false;
  protected accessor tooltip_: string =
      loadTimeData.getString(RELOAD_BUTTON_TOOLTIP_RELOAD);

  // TODO(crbug.com/444358999): implement the reload logic
  protected onReloadOrStopClick_(e: MouseEvent) {
    if (this.isLoading_) {
      BrowserProxyImpl.getInstance().handler.stopReload();
    } else {
      // If the shift or ctrl key is pressed, we should reload with cache
      // ignored.
      BrowserProxyImpl.getInstance().handler.reload(e.shiftKey || e.ctrlKey);
    }
    // Update the renderer in advance to avoid the delay.
    this.isLoading_ = !this.isLoading_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reload-button-app': ReloadButtonAppElement;
  }
}

customElements.define(ReloadButtonAppElement.is, ReloadButtonAppElement);
