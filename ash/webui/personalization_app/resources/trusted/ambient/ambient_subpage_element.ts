// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ambient-subpage component displays the main content of
 * the ambient mode settings.
 */

import './albums_subpage_element.js';
import './ambient_weather_element.js';
import './toggle_row_element.js';
import './topic_source_list_element.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AmbientModeAlbum, TemperatureUnit, TopicSource} from '../personalization_app.mojom-webui.js';
import {Paths} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientObserver} from './ambient_observer.js';
import {ToggleRow} from './toggle_row_element.js';

export class AmbientSubpage extends WithPersonalizationStore {
  static get is() {
    return 'ambient-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      path: Paths,
      queryParams: Object,
      albums_: {
        type: Array,
        value: null,
      },
      ambientModeEnabled_: Boolean,
      temperatureUnit_: Number,
      topicSource_: Number,
    };
  }

  path: Paths;
  queryParams: Record<string, string>;
  private albums_: AmbientModeAlbum[]|null = null;
  private ambientModeEnabled_: boolean;
  private temperatureUnit_: TemperatureUnit|null = null;
  private topicSource_: TopicSource|null = null;

  connectedCallback() {
    super.connectedCallback();
    AmbientObserver.initAmbientObserverIfNeeded();
    this.watch<AmbientSubpage['albums_']>(
        'albums_', state => state.ambient.albums);
    this.watch<AmbientSubpage['ambientModeEnabled_']>(
        'ambientModeEnabled_', state => state.ambient.ambientModeEnabled);
    this.watch<AmbientSubpage['temperatureUnit_']>(
        'temperatureUnit_', state => state.ambient.temperatureUnit);
    this.watch<AmbientSubpage['topicSource_']>(
        'topicSource_', state => state.ambient.topicSource);
    this.updateFromStore();
  }

  private onClickAmbientModeButton_(event: Event) {
    event.stopPropagation();
    this.setAmbientModeEnabled_(!this.ambientModeEnabled_);
  }

  private onToggleStateChanged_(event: Event) {
    const toggleRow = event.currentTarget as ToggleRow;
    const ambientModeEnabled = toggleRow!.checked;
    this.setAmbientModeEnabled_(ambientModeEnabled);
  }

  private setAmbientModeEnabled_(ambientModeEnabled: boolean) {
    setAmbientModeEnabled(
        ambientModeEnabled, getAmbientProvider(), this.getStore());
  }

  private temperatureUnitToString_(temperatureUnit: TemperatureUnit): string {
    return temperatureUnit != null ? temperatureUnit.toString() : '';
  }

  private hasGooglePhotosAlbums_(): boolean {
    return (this.albums_ || [])
        .some(album => album.topicSource === TopicSource.kGooglePhotos);
  }

  private getTopicSource_(): TopicSource|null {
    if (!this.queryParams) {
      return null;
    }

    const topicSource = parseInt(this.queryParams['topicSource'], 10);
    if (isNaN(topicSource)) {
      return null;
    }

    return topicSource;
  }

  private getAlbums_(): AmbientModeAlbum[] {
    if (!this.queryParams) {
      return [];
    }

    const topicSource = this.getTopicSource_();
    return (this.albums_ || []).filter(album => {
      return album.topicSource === topicSource;
    });
  }

  private shouldShowMainSettings_(path: Paths): boolean {
    return path === Paths.Ambient;
  }

  private shouldShowAlbums_(path: Paths): boolean {
    return path === Paths.AmbientAlbums;
  }
}

customElements.define(AmbientSubpage.is, AmbientSubpage);
