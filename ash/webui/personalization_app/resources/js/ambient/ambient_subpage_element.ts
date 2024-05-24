// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ambient-subpage component displays the main content of
 * the ambient mode settings.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import './albums_subpage_element.js';
import './ambient_preview_small_element.js';
import './ambient_theme_list_element.js';
import './ambient_weather_element.js';
import './toggle_row_element.js';
import './topic_source_list_element.js';

import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AmbientModeAlbum, AmbientTheme, TemperatureUnit, TopicSource} from '../../personalization_app.mojom-webui.js';
import {isAmbientModeAllowed} from '../load_time_booleans.js';
import {Paths, ScrollableTarget} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {dismissTimeOfDayBanner, setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientObserver} from './ambient_observer.js';
import {getTemplate} from './ambient_subpage_element.html.js';

export class AmbientSubpageElement extends WithPersonalizationStore {
  static get is() {
    return 'ambient-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: Paths,
      queryParams: Object,
      albums_: {
        type: Array,
        value: null,
      },
      ambientTheme_: {
        type: Object,
        value: null,
      },
      ambientModeEnabled_: {
        type: Boolean,
        value: null,
        observer: 'onAmbientModeEnabledChanged_',
      },
      duration_: {
        type: Number,
        value: null,
      },
      temperatureUnit_: {
        type: Number,
        value: null,
      },
      topicSource_: {
        type: Number,
        value: null,
      },
      loading_: {
        type: Boolean,
        computed:
            'computeLoading_(ambientModeEnabled_, albums_, temperatureUnit_, topicSource_, isOnline_)',
        observer: 'onLoadingChanged_',
      },
      isOnline_: {
        type: Boolean,
        value() {
          return window.navigator.onLine;
        },
      },
    };
  }

  path: Paths;
  queryParams: Record<string, string>;
  private albums_: AmbientModeAlbum[]|null;
  private ambientModeEnabled_: boolean|null;
  private ambientTheme_: AmbientTheme|null;
  private duration_: number|null;
  private temperatureUnit_: TemperatureUnit|null;
  private topicSource_: TopicSource|null;
  private isOnline_: boolean;

  // Refetch albums if the user is currently viewing ambient subpage, focuses
  // another window, and then re-focuses personalization app.
  private onFocus_ = () => getAmbientProvider().fetchSettingsAndAlbums();

  override ready() {
    // Pre-scroll to prevent visual jank when focusing the toggle row.
    window.scrollTo(0, 0);
    super.ready();
    afterNextRender(this, () => {
      const elem = this.shadowRoot!.getElementById('ambientToggleRow');
      if (elem) {
        // Focus the toggle row to inform screen reader users of the current
        // state.
        elem.focus();
      }
    });

    window.addEventListener('online', () => {
      this.isOnline_ = true;
    });
    window.addEventListener('offline', () => {
      this.isOnline_ = false;
    });
  }

  override connectedCallback() {
    assert(
        isAmbientModeAllowed(),
        'ambient subpage should not load if ambient not allowed');

    super.connectedCallback();
    AmbientObserver.initAmbientObserverIfNeeded();
    this.watch<AmbientSubpageElement['albums_']>(
        'albums_', state => state.ambient.albums);
    this.watch<AmbientSubpageElement['ambientModeEnabled_']>(
        'ambientModeEnabled_', state => state.ambient.ambientModeEnabled);
    this.watch<AmbientSubpageElement['ambientTheme_']>(
        'ambientTheme_', state => state.ambient.ambientTheme);
    this.watch<AmbientSubpageElement['temperatureUnit_']>(
        'temperatureUnit_', state => state.ambient.temperatureUnit);
    this.watch<AmbientSubpageElement['topicSource_']>(
        'topicSource_', state => state.ambient.topicSource);
    this.watch<AmbientSubpageElement['duration_']>(
        'duration_', state => state.ambient.duration);
    this.updateFromStore();

    getAmbientProvider().setPageViewed();

    window.addEventListener('focus', this.onFocus_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('focus', this.onFocus_);
  }

  // Scroll down to the topic source list.
  private scrollToTopicSourceList_() {
    const elem = this.shadowRoot!.querySelector('topic-source-list');
    if (elem) {
      elem.scrollIntoView();
      elem.focus();
    }
  }

  private onAmbientModeEnabledChanged_(value: boolean) {
    if (value) {
      // Dismisses the banner after the user visits this subpage and ambient
      // mode is enabled.
      dismissTimeOfDayBanner(this.getStore());
    }
  }

  private onLoadingChanged_(value: boolean) {
    if (!value && !!this.queryParams &&
        this.queryParams['scrollTo'] === ScrollableTarget.TOPIC_SOURCE_LIST) {
      afterNextRender(this, () => this.scrollToTopicSourceList_());
    }
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

  // Null result indicates albums are loading.
  private getAlbums_(): AmbientModeAlbum[]|null {
    if (!this.queryParams || this.albums_ === null) {
      return null;
    }

    const topicSource = this.getTopicSource_();
    return (this.albums_ || []).filter(album => {
      return album.topicSource === topicSource;
    });
  }

  private shouldShowMainSettings_(path: Paths): boolean {
    return path === Paths.AMBIENT;
  }

  private shouldShowAlbums_(path: Paths): boolean {
    return path === Paths.AMBIENT_ALBUMS;
  }

  private computeLoading_(): boolean {
    return this.ambientModeEnabled_ === null || this.albums_ === null ||
        this.topicSource_ === null || this.temperatureUnit_ === null ||
        this.duration_ === null || !this.isOnline_;
  }

  private getPlaceholders_(x: number): number[] {
    return new Array(x).fill(0);
  }
}

customElements.define(AmbientSubpageElement.is, AmbientSubpageElement);
