// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import './icons.html.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {Skill} from './skill.mojom-webui.js';
import {SkillSource} from './skill.mojom-webui.js';
import {getCss} from './card.css.js';
import {getHtml} from './card.html.js';

export interface SkillCardElement {
  $: {
    cardBody: HTMLElement,
    name: HTMLElement,
    icon: HTMLElement,
  };
}

export class SkillCardElement extends CrLitElement {
  static get is() {
    return 'skill-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      skill: {type: Object},
    };
  }

  accessor skill: Skill = {
    id: '',
    name: '',
    icon: '',
    prompt: '',
    // Default to user created since these are added by the user via the UI.
    source: SkillSource.kUserCreated,
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'skill-card': SkillCardElement;
  }
}

customElements.define(SkillCardElement.is, SkillCardElement);
