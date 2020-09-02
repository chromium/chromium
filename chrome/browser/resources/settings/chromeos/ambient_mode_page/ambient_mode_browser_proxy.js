// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// #import {AmbientModeTopicSource, AmbientModeTemperatureUnit, AmbientModeSettings} from './constants.m.js';
// clang-format on

/**
 * @fileoverview A helper object used from the ambient mode section to interact
 * with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  /* #export */ class AmbientModeBrowserProxy {
    /**
     * Retrieves the AmbientModeTopicSource and AmbientModeTemperatureUnit from
     * server. As a response, the C++ sends the 'topic-source-changed' and
     * 'temperature-unit-changed' events.
     */
    requestSettings() {}

    /**
     * Retrieves the albums from server. As a response, the C++ sends either the
     * 'albums-changed' WebUIListener event.
     * @param {!AmbientModeTopicSource} topicSource the topic source for which
     *     the albums requested.
     */
    requestAlbums(topicSource) {}

    /**
     * Updates the selected temperature unit to server.
     * @param {!AmbientModeTemperatureUnit} temperatureUnit
     */
    setSelectedTemperatureUnit(temperatureUnit) {}

    /**
     * Updates the selected albums of Google Photos or art categories to server.
     * @param {!AmbientModeSettings} settings the selected albums or categeries.
     */
    setSelectedAlbums(settings) {}
  }

  /** @implements {settings.AmbientModeBrowserProxy} */
  /* #export */ class AmbientModeBrowserProxyImpl {
    /** @override */
    requestSettings() {
      chrome.send('requestSettings');
    }

    /** @override */
    requestAlbums(topicSource) {
      chrome.send('requestAlbums', [topicSource]);
    }

    /** @override */
    setSelectedTemperatureUnit(temperatureUnit) {
      chrome.send('setSelectedTemperatureUnit', [temperatureUnit]);
    }

    /** @override */
    setSelectedAlbums(settings) {
      chrome.send('setSelectedAlbums', [settings]);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(AmbientModeBrowserProxyImpl);

  // #cr_define_end
  return {
    AmbientModeBrowserProxy: AmbientModeBrowserProxy,
    AmbientModeBrowserProxyImpl: AmbientModeBrowserProxyImpl,
  };
});
