// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {FakeMethodResolver} from '../fake_method_resolver.js';

import {CrosHotspotConfigInterface, CrosHotspotConfigObserverRemote, HotspotAllowStatus, HotspotConfig, HotspotControlResult, HotspotEnabledStateObserverRemote, HotspotInfo, HotspotState, SetHotspotConfigResult} from './cros_hotspot_config.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the CrosHotspotConfig mojo interface.
 */

export class FakeHotspotConfig implements CrosHotspotConfigInterface {
  private methods_: FakeMethodResolver = new FakeMethodResolver();
  private hotspotInfo_: HotspotInfo|null = null;
  private observers_: CrosHotspotConfigObserverRemote[] = [];
  private stateObservers_: HotspotEnabledStateObserverRemote[] = [];
  private setHotspotConfigResult_: SetHotspotConfigResult|null = null;
  private enableHotspotResult_: HotspotControlResult|null = null;
  private disableHotspotResult_: HotspotControlResult|null = null;

  constructor() {
    this.registerMethods();
  }

  // Implements CrosHotspotConfigInterface.getHotspotInfo().
  getHotspotInfo(): Promise<{hotspotInfo: HotspotInfo}> {
    this.methods_.setResult('getHotspotInfo', {hotspotInfo: this.hotspotInfo_});
    return this.methods_.resolveMethod('getHotspotInfo');
  }

  // Set the value that will be returned when calling getHotspotInfo() and
  // notify observers.
  setFakeHotspotInfo(hotspotInfo: HotspotInfo): void {
    this.hotspotInfo_ = hotspotInfo;
    this.notifyHotspotInfoUpdated_();
  }

  // Update the hotspot state value and notify observers.
  setFakeHotspotState(state: HotspotState): void {
    assert(this.hotspotInfo_);
    this.hotspotInfo_.state = state;
    this.hotspotInfo_ = {...this.hotspotInfo_};
    this.notifyHotspotInfoUpdated_();
  }

  // Update the hotspot allow status and notify observers.
  setFakeHotspotAllowStatus(allowStatus: HotspotAllowStatus): void {
    assert(this.hotspotInfo_);
    this.hotspotInfo_.allowStatus = allowStatus;
    this.hotspotInfo_ = {...this.hotspotInfo_};
    this.notifyHotspotInfoUpdated_();
  }

  // Update the hotspot connected client count and notify observers.
  setFakeHotspotActiveClientCount(clientCount: number): void {
    assert(this.hotspotInfo_);
    this.hotspotInfo_.clientCount = clientCount;
    this.hotspotInfo_ = {...this.hotspotInfo_};
    this.notifyHotspotInfoUpdated_();
  }

  // Update the hotspot config and notify observers.
  setFakeHotspotConfig(config: HotspotConfig|undefined): void {
    assert(this.hotspotInfo_);
    this.hotspotInfo_.config = config !== undefined ? config : null;
    this.hotspotInfo_ = {...this.hotspotInfo_};
    this.notifyHotspotInfoUpdated_();
  }

  // Set the value that will be returned when calling enableHotspot().
  setFakeEnableHotspotResult(result: HotspotControlResult): void {
    this.enableHotspotResult_ = result;
    this.methods_.setResult(
        'enableHotspot', {result: this.enableHotspotResult_});
  }

  // Implements CrosHotspotConfigInterface.enableHotspot().
  enableHotspot(): Promise<{result: HotspotControlResult}> {
    assert(this.hotspotInfo_);
    if (this.hotspotInfo_.state === HotspotState.kEnabled) {
      return this.methods_.resolveMethod('enableHotspot');
    }
    this.setFakeHotspotState(HotspotState.kEnabling);

    if (this.enableHotspotResult_ === HotspotControlResult.kSuccess) {
      this.setFakeHotspotState(HotspotState.kEnabled);
    } else {
      this.setFakeHotspotState(HotspotState.kDisabled);
    }

    return this.methods_.resolveMethod('enableHotspot');
  }

  // Set the value that will be returned when calling disableHotspot().
  setFakeDisableHotspotResult(result: HotspotControlResult): void {
    this.disableHotspotResult_ = result;
    this.methods_.setResult('disableHotspot', {result: result});
  }

  // Implements CrosHotspotConfigInterface.disableHotspot().
  disableHotspot(): Promise<{result: HotspotControlResult}> {
    assert(this.hotspotInfo_);
    if (this.hotspotInfo_.state === HotspotState.kDisabled) {
      return this.methods_.resolveMethod('disableHotspot');
    }
    this.setFakeHotspotState(HotspotState.kDisabling);

    if (this.disableHotspotResult_ === HotspotControlResult.kSuccess) {
      this.setFakeHotspotState(HotspotState.kDisabled);
    } else {
      this.setFakeHotspotState(HotspotState.kEnabled);
    }

    return this.methods_.resolveMethod('disableHotspot');
  }

  // Set the value that will be returned when calling setHotspotConfig().
  setFakeSetHotspotConfigResult(result: SetHotspotConfigResult): void {
    this.setHotspotConfigResult_ = result;
    this.methods_.setResult(
        'setHotspotConfig', {result: this.setHotspotConfigResult_});
  }

  // Implements CrosHotspotConfigInterface.setHotspotConfig().
  setHotspotConfig(hotspotConfig: HotspotConfig):
      Promise<{result: SetHotspotConfigResult}> {
    if (this.setHotspotConfigResult_ === SetHotspotConfigResult.kSuccess) {
      this.setFakeHotspotConfig(hotspotConfig);
    }

    return this.methods_.resolveMethod('setHotspotConfig');
  }

  // Implements CrosHotspotConfigInterface.addObserver().
  addObserver(remote: CrosHotspotConfigObserverRemote): void {
    this.observers_.push(remote);
  }

  // Implements CrosHotspotConfigInterface.observeEnabledStateChanges()
  observeEnabledStateChanges(remote: HotspotEnabledStateObserverRemote): void {
    this.stateObservers_.push(remote);
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

    this.hotspotInfo_ = null;
    this.setHotspotConfigResult_ = null;
    this.enableHotspotResult_ = null;
    this.disableHotspotResult_ = null;
    this.observers_ = [];
  }

  private notifyHotspotInfoUpdated_(): void {
    for (const observer of this.observers_) {
      observer.onHotspotInfoChanged();
    }
  }
}
