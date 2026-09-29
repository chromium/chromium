// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_hero.css.js';
import {getHtml} from './topic_hero.html.js';
import {getBackgroundColorForTopic, getBadgePath, getBadgeShapeForTopic, isCrIcon} from './topic_utils.js';
import type {TopicItem} from './topic_utils.js';

// The decorative banner at the top of the topic details page: the topic's
// badge shape and emoji tiled over its badge color, matching the `topic-card`
// it was opened from. Purely decorative, so hidden from assistive technology.
export class TopicHeroElement extends CrLitElement {
  static get is() {
    return 'topic-hero';
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

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('aria-hidden', 'true');
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('topic')) {
      this.style.setProperty(
          '--topic-hero-background-color',
          getBackgroundColorForTopic(this.topic?.id || ''));
    }
  }

  protected getBadgePath_(): string {
    return getBadgePath(getBadgeShapeForTopic(this.topic?.id || ''));
  }

  // cr-icon identifiers can't be drawn inside an SVG pattern, so only emoji
  // are tiled.
  protected getTextIcon_(): string {
    const icon = this.topic?.icon || '';
    return isCrIcon(icon) ? '' : icon;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-hero': TopicHeroElement;
  }
}

customElements.define(TopicHeroElement.is, TopicHeroElement);
