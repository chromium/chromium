// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ambient-subpage component displays the main content of
 * the ambient mode settings.
 */

import 'chrome://personalization/trusted/ambient/toggle_row_element.js';
import 'chrome://personalization/trusted/ambient/topic_source_list_element.js';

import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientObserver} from './ambient_observer.js';
import {ToggleRow} from './toggle_row_element.js';
import {TopicSourceList} from './topic_source_list_element.js';

export class AmbientSubpage extends WithPersonalizationStore {
  static get is() {
    return 'ambient-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      ambientModeEnabled_: Boolean,
      hasGooglePhotosAlbums_: {type: Boolean, value: true},
      topicSource_: Number,
    };
  }

  private ambientModeEnabled_: boolean;
  private hasGooglePhotosAlbums_: boolean;
  private topicSource_: TopicSource|null = null;

  connectedCallback() {
    super.connectedCallback();
    AmbientObserver.initAmbientObserverIfNeeded();
    this.watch<AmbientSubpage['ambientModeEnabled_']>(
        'ambientModeEnabled_', state => state.ambient.ambientModeEnabled);
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
}

customElements.define(AmbientSubpage.is, AmbientSubpage);
