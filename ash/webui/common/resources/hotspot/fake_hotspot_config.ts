// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {CrosHotspotConfigInterface, CrosHotspotConfigObserverRemote, HotspotConfig, HotspotControlResult, HotspotInfo, SetHotspotConfigResult} from './cros_hotspot_config.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the CrosHotspotConfig mojo interface.
 */

export class FakeHotspotConfig implements CrosHotspotConfigInterface {
  private methods_: FakeMethodResolver = new FakeMethodResolver();
  private hotspotInfo_: HotspotInfo|null = null;

  constructor() {
    this.registerMethods();
  }

  getHotspotInfo(): Promise<{hotspotInfo: HotspotInfo}> {
    this.methods_.setResult('getHotspotInfo', {hotspotInfo: this.hotspotInfo_});
    return this.methods_.resolveMethod('getHotspotInfo');
  }

  // Sets the value that will be returned when calling getHotspotInfo().
  setFakeHotspotInfo(hotspotInfo: HotspotInfo): void {
    this.hotspotInfo_ = hotspotInfo;
  }

  enableHotspot(): Promise<{result: HotspotControlResult}> {
    return this.methods_.resolveMethod('enableHotspot');
  }

  // Sets the value that will be returned when calling enableHotspot().
  setFakeEnableHotspotResult(result: HotspotControlResult): void {
    this.methods_.setResult('enableHotspot', {result: result});
  }

  disableHotspot(): Promise<{result: HotspotControlResult}> {
    return this.methods_.resolveMethod('disableHotspot');
  }

  // Sets the value that will be returned when calling disableHotspot().
  setFakeDisableHotspotResult(result: HotspotControlResult): void {
    this.methods_.setResult('disableHotspot', {result: result});
  }

  setHotspotConfig(hotspotConfig: HotspotConfig):
      Promise<{result: SetHotspotConfigResult}> {
    assert(this.hotspotInfo_ !== null);
    this.hotspotInfo_.config = hotspotConfig;
    return this.methods_.resolveMethod('setHotspotConfig');
  }

  // Sets the value that will be returned when calling enableHotspot().
  setFakeSetHotspotConfigResult(result: SetHotspotConfigResult): void {
    this.methods_.setResult('setHotspotConfig', {result: result});
  }

  addObserver(remote: CrosHotspotConfigObserverRemote): void {
    // TODO(b/239477916): implementation
    assert(remote !== null);
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods_.register('getHotspotInfo');
    this.methods_.register('enableHotspot');
    this.methods_.register('disableHotspot');
    this.methods_.register('setHotspotConfig');
  }

  // Disables all observers and resets config to its initial state.
  reset(): void {
    this.methods_ = new FakeMethodResolver();
    this.registerMethods();
  }
}
