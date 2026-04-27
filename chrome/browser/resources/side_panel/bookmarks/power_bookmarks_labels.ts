// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './icons.html.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './power_bookmarks_labels.html.js';
import type {Label} from './power_bookmarks_service.js';

export class PowerBookmarksLabelsElement extends CrLitElement {
  static get is() {
    return 'power-bookmarks-labels';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      labels: {
        type: Array,
        notify: true,
      },
      trackedProductInfos: {type: Object},
    };
  }

  accessor disabled: boolean = false;
  accessor labels: Label[] = [];
  accessor trackedProductInfos: {[key: string]: BookmarkProductInfo} = {};

  override willUpdate(changedProperties: PropertyValues) {
    super.willUpdate(changedProperties as PropertyValues<this>);
    if (changedProperties.has('trackedProductInfos')) {
      this.labels = this.computeLabels();
    }
  }

  private computeLabels(): Label[] {
    const labels: Label[] = [];
    const hasTrackedProducts =
        Object.keys(this.trackedProductInfos)
            .some(key => this.trackedProductInfos[key] !== null);
    if (hasTrackedProducts) {
      // Reuse the current price tracking label if one exists, to maintain its
      // active state.
      const currentLabel = this.labels[0];
      labels.push(Object.assign(
          {}, {
            label: loadTimeData.getString('priceTrackingLabel'),
            icon: 'bookmarks:price-tracking',
            active: false,
          },
          {active: currentLabel ? currentLabel.active : false}));
    }
    return labels;
  }

  protected getLabelIcon(label: Label): string {
    return label.active ? 'bookmarks:check' : label.icon;
  }

  protected onLabelClick(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    const index = Number((event.currentTarget as HTMLElement).dataset['index']);
    const labels = [...this.labels];
    labels[index] = {...labels[index], active: !labels[index].active};
    this.labels = labels;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-labels': PowerBookmarksLabelsElement;
  }
}

customElements.define(
    PowerBookmarksLabelsElement.is, PowerBookmarksLabelsElement);
