// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './components/common_styles/oobe_dialog_host_styles.css.js';
import './components/dialogs/oobe_adaptive_dialog.js';
import './components/oobe_icons.html.js';
import './components/common_styles/oobe_common_styles.css.js';
import './icons.html.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './curtain_screen.html.js';

function setDialogSizeAndOrientation() {
  document.documentElement.style.setProperty(
      '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
  document.documentElement.style.setProperty(
      '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');

  // Screen orientation value needs to be kept up-to-date.
  document.documentElement.setAttribute(
      'orientation',
      window.innerWidth > window.innerHeight ? 'horizontal' : 'vertical');
}

// TODO(b/274059668): Remove when OOBE is migrated to TS.
type OobeAdaptiveDialog = HTMLElement&{
  onBeforeShow(): void,
};

// Inform the compiler that the `this.$.mainCurtainDialog` statement below
// returns an element of type `OobeAdaptiveDialog`.
interface CurtainScreenElement {
  $: {
    mainCurtainDialog: OobeAdaptiveDialog,
  };
}

class CurtainScreenElement extends PolymerElement {
  static get is() {
    return 'curtain-screen' as const;
  }

  static get template() {
    return getTemplate();
  }

  override ready() {
    super.ready();

    this.$.mainCurtainDialog.onBeforeShow();
    setDialogSizeAndOrientation();

    // TODO(b/268463435): Move include directly to the oobe.html after Jelly
    // flag will be enabled by default.
    const fontLink = document.createElement('link');
    fontLink.rel = 'stylesheet';
    fontLink.href = 'chrome://theme/typography.css';
    document.head.appendChild(fontLink);

    // Required on body to apply cros_color_overrides
    document.body.classList.add('jelly-enabled');

    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', this.listenToColorChanges);
    } else {
      this.listenToColorChanges();
    }
  }

  listenToColorChanges() {
    // Start listening for color changes in 'chrome://theme/colors.css'. Force
    // reload it once to account for any missed color change events between
    // loading oobe.html and here.
    const updater = ColorChangeUpdater.forDocument();
    updater.start();
    updater.refreshColorsCss();
  }

  override connectedCallback() {
    super.connectedCallback();
    window.addEventListener('resize', setDialogSizeAndOrientation);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('resize', setDialogSizeAndOrientation);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CurtainScreenElement.is]: CurtainScreenElement;
  }
}

customElements.define(CurtainScreenElement.is, CurtainScreenElement);
