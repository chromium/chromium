// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './icons.html.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmarks_labels.html.js';
import type {Label} from './power_bookmarks_service.js';

export class PowerBookmarksLabelsElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-labels';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,
      labels: {
        type: Array,
        computed: 'computeLabels(trackedProductInfos.*)',
        notify: true,
      },
      trackedProductInfos: Object,
    };
  }

  disabled: boolean = false;
  labels: Label[] = [];
  trackedProductInfos: {[key: string]: BookmarkProductInfo} = {};

  private computeLabels() {
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

  private getLabelIcon(label: Label): string {
    return label.active ? 'bookmarks:check' : label.icon;
  }

  private onLabelClick(event: DomRepeatEvent<Label>) {
    event.preventDefault();
    event.stopPropagation();
    this.set(`labels.${event.model.index}.active`, !event.model.item.active);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-labels': PowerBookmarksLabelsElement;
  }
}

customElements.define(
    PowerBookmarksLabelsElement.is, PowerBookmarksLabelsElement);
