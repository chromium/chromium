// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_card.css.js';
import {getHtml} from './topic_card.html.js';
import {getBackgroundColorForTopic, getBadgePath, getBadgeShapeForTopic, isCrIcon} from './topic_utils.js';
import type {TopicItem} from './topic_utils.js';

// TODO(crbug.com/558572977): Use internationalized strings once GRD
// strings are added.
export class TopicCardElement extends CrLitElement {
  static get is() {
    return 'topic-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topic: {type: Object},
    };
  }

  accessor topic: TopicItem|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('topic')) {
      this.toggleAttribute('hidden', !this.hasContent_());
    }
  }

  protected hasContent_(): boolean {
    return Boolean(
        this.topic &&
        (this.topic.title.trim() || this.topic.description.trim()));
  }

  protected getBadgePath_(): string {
    return getBadgePath(getBadgeShapeForTopic(this.topic?.id || ''));
  }

  protected getBackgroundColor_(): string {
    return getBackgroundColorForTopic(this.topic?.id || '');
  }

  protected getIcon_(): string {
    return this.topic?.icon || '';
  }

  protected isCrIcon_(): boolean {
    return isCrIcon(this.getIcon_());
  }

  protected getActionAriaLabel_(): string {
    return this.topic?.title.trim() ? `Jump back in to ${this.topic.title}` :
                                      'Jump back in';
  }

  protected onJumpBackInClick_() {
    if (!this.topic) {
      return;
    }
    this.fire('jump-back-in', {topic: this.topic});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-card': TopicCardElement;
  }
}

customElements.define(TopicCardElement.is, TopicCardElement);
