// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_a11y_announcer_demo.css.js';
import {getHtml} from './cr_a11y_announcer_demo.html.js';

export interface CrA11yAnnouncerDemoElement {
  $: {
    announcerContainer: HTMLElement,
  };
}

export class CrA11yAnnouncerDemoElement extends CrLitElement {
  static get is() {
    return 'cr-a11y-announcer-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      forceShowAnnouncer_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  private announcementTextCount_: number = 0;
  protected accessor forceShowAnnouncer_: boolean = false;

  protected onAnnounceTextClick_() {
    const announcer = this.forceShowAnnouncer_ ?
        getAnnouncerInstance(this.$.announcerContainer) :
        getAnnouncerInstance();
    announcer.announce(`Announcement number ${++this.announcementTextCount_}`);
  }

  protected onAnnounceMultipleTextsClick_() {
    const announcer = this.forceShowAnnouncer_ ?
        getAnnouncerInstance(this.$.announcerContainer) :
        getAnnouncerInstance();
    announcer.announce('Page is loading...');
    announcer.announce('Page has loaded.');
  }

  protected onForceShowAnnouncerChanged_(e: CustomEvent<{value: boolean}>) {
    this.forceShowAnnouncer_ = e.detail.value;
  }
}

export const tagName = CrA11yAnnouncerDemoElement.is;

customElements.define(
    CrA11yAnnouncerDemoElement.is, CrA11yAnnouncerDemoElement);
