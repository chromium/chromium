// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {CalibrationComponentStatus, CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, Component, ComponentType, ErrorObserverRemote, ExternalDiskStateObserverRemote, FeatureLevel, FinalizationError, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningError, ProvisioningObserverRemote, ProvisioningStatus, RmadErrorCode, Shimless3pDiagnosticsAppInfo, ShimlessRmaServiceInterface, Show3pDiagnosticsAppResult, ShutdownMethod, State, StateResult, UpdateErrorCode, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from './shimless_rma.mojom-webui.js';


/**
 * Type for methods needed for the fake FakeShimlessRmaService implementation.
 */
export type FakeShimlessRmaServiceInterface = ShimlessRmaServiceInterface&{
  setStates(states: StateResult[]): void,
  setAsyncOperationDelayMs(delayMs: number): void,
  setAbortRmaResult(error: RmadErrorCode): void,
  enableAutomaticallyTriggerProvisioningObservation(): void,
  getCurrentOsVersion(): void,
  setCheckForOsUpdatesResult(version: string): void,
  setUpdateOsResult(started: boolean): void,
  setGetRsuDisableWriteProtectChallengeResult(challenge: string): void,
  enableAutomaticallyTriggerDisableWriteProtectionObservation(): void,
  setGetPowerwashRequiredResult(powerwashRequired: boolean): void,
  setSaveLogResult(savePath: FilePath): void,
  enableAutomaticallyTriggerHardwareVerificationStatusObservation(): void,
  setGetCurrentOsVersionResult(version: string|null): void,
  setGetComponentListResult(components: Component[]): void,
  setGetRsuDisableWriteProtectHwidResult(hwid: string): void,
  getRsuDisableWriteProtectChallengeQrCode(): Promise<{qrCodeData: number[]}>,
  setGetRsuDisableWriteProtectChallengeQrCodeResponse(qrCodeData: number[]):
      void,
  setGetLogResult(log: string): void,
  enableAutomaticallyTriggerFinalizationObservation(): void,
  enableAutomaticallyTriggerOsUpdateObservation(): void,
  setGetWriteProtectDisableCompleteAction(
      action: WriteProtectDisableCompleteAction): void,
  getWriteProtectDisableCompleteAction():
      Promise<{action: WriteProtectDisableCompleteAction}>,
  getOriginalSerialNumber(): Promise<{serialNumber: string}>,
  setGetOriginalSerialNumberResult(serialNumber: string): void,
  getRegionList(): Promise<{regions: string[]}>,
  setGetRegionListResult(regions: string[]): void,
  getCalibrationComponentList():
      Promise<{components: CalibrationComponentStatus[]}>,
  setGetCalibrationComponentListResult(
      components: CalibrationComponentStatus[]): void,
  setGetSkuListResult(skus: bigint[]): void,
  setGetSkuDescriptionListResult(skuDescriptions: string[]): void,
  enableAautomaticallyTriggerUpdateRoFirmwareObservation(): void,
  setGetOriginalSkuResult(skuIndex: number): void,
  enableAutomaticallyTriggerCalibrationObservation(): void,
  getCustomLabelList(): Promise<{customLabels: string[]}>,
  enableAutomaticallyTriggerPowerCableStateObservation(): void,
  setGetOriginalRegionResult(regionIndex: number): void,
  setGetCustomLabelListResult(customLabels: string[]): void,
  setGetOriginalCustomLabelResult(customLabelIndex: number): void,
  getOriginalDramPartNumber(): Promise<{dramPartNumber: string}>,
  setGetOriginalDramPartNumberResult(dramPartNumber: string): void,
  setGetOriginalFeatureLevelResult(featureLevel: FeatureLevel): void,
  setGetCalibrationSetupInstructionsResult(
      instructions: CalibrationSetupInstruction): void,
};


export class FakeShimlessRmaService implements FakeShimlessRmaServiceInterface {
  constructor() {
    this.methods = new FakeMethodResolver();
    this.observables = new FakeObservables();

    /**
     * The list of states for this RMA flow.
     */
    this.states = [];

    /**
     * The index into states for the current fake state.
     */
    this.stateIndex = 0;

    /**
     * The list of components.
     */
    this.components = [];

    /**
     * Control automatically triggering a HWWP disable observation.
     */
    this.automaticallyTriggerDisableWriteProtectionObservation = false;

    /**
     * Control automatically triggering update RO firmware observations.
     */
    this.automaticallyTriggerUpdateRoFirmwareObservation = false;

    /**
     * Control automatically triggering provisioning observations.
     */
    this.automaticallyTriggerProvisioningObservation = false;

    /**
     * Control automatically triggering calibration observations.
     */
    this.automaticallyTriggerCalibrationObservation = false;

    /**
     * Control automatically triggering OS update observations.
     */
    this.automaticallyTriggerOsUpdateObservation = false;

    /**
     * Control automatically triggering a hardware verification observation.
     */
    this.automaticallyTriggerHardwareVerificationStatusObservation = false;

    /**
     * Control automatically triggering a finalization observation.
     */
    this.automaticallyTriggerFinalizationObservation = false;

    /**
     * Control automatically triggering power cable state observations.
     */
    this.automaticallyTriggerPowerCableStateObservation = false;

    /**
     * Both abortRma and forward state transitions can have significant delays
     * that are useful to fake for manual testing.
     * Defaults to no delay for unit tests.
     */
    this.resolveMethodDelayMs = 0;

    /**
     * The result of calling trackConfiguredNetworks.
     */
    this.trackConfiguredNetworksCalled = false;

    /**
     * The approval of last call to completeLast3pDiagnosticsInstallation.
     */
    this.lastCompleteLast3pDiagnosticsInstallationApproval = null;

    /**
     * Has show3pDiagnosticsApp been called.
     */
    this.wasShow3pDiagnosticsAppCalled = false;

    this.reset();
  }

  private methods: FakeMethodResolver;
  private observables: FakeObservables;
  private states: StateResult[];
  private stateIndex: number;
  private components: Component[];
  private automaticallyTriggerDisableWriteProtectionObservation: boolean;
  private automaticallyTriggerUpdateRoFirmwareObservation: boolean;
  private automaticallyTriggerProvisioningObservation: boolean;
  private automaticallyTriggerCalibrationObservation: boolean;
  private automaticallyTriggerOsUpdateObservation: boolean;
  private automaticallyTriggerHardwareVerificationStatusObservation: boolean;
  private automaticallyTriggerFinalizationObservation: boolean;
  private automaticallyTriggerPowerCableStateObservation: boolean;
  private resolveMethodDelayMs: number;
  private trackConfiguredNetworksCalled: boolean;
  private lastCompleteLast3pDiagnosticsInstallationApproval: boolean|null;
  private wasShow3pDiagnosticsAppCalled: boolean;

  setAsyncOperationDelayMs(delayMs: number) {
    this.resolveMethodDelayMs = delayMs;
  }

  /**
   * Set the ordered list of states end error codes for this fake.
   * Setting an empty list (the default) returns kRmaNotRequired for any state
   * function.
   * Next state functions and transitionPreviousState will move through the fake
   * state through the list, and return kTransitionFailed if it would move off
   * either end. getCurrentState always return the state at the current index.
   */
  setStates(states: StateResult[]) {
    this.states = states;
    this.stateIndex = 0;
  }

  getCurrentState(): Promise<{stateResult: StateResult}> {
    // As next state functions and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states.length === 0) {
      this.setFakeCurrentState(
          State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else {
      // It should not be possible for stateIndex to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex < this.states.length);
      const state = this.states[this.stateIndex];
      this.setFakeCurrentState(
          state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods.resolveMethodWithDelay(
        'getCurrentState', this.resolveMethodDelayMs);
  }

  transitionPreviousState(): Promise<{stateResult: StateResult}> {
    // As next state methods and transitionPreviousState can modify the result
    // of this function the result must be set at the time of the call.
    if (this.states.length === 0) {
      this.setFakePrevState(
          State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex === 0) {
      // It should not be possible for stateIndex to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex < this.states.length);
      const state = this.states[this.stateIndex];
      this.setFakePrevState(
          state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else {
      this.stateIndex--;
      const state = this.states[this.stateIndex];
      this.setFakePrevState(
          state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods.resolveMethodWithDelay(
        'transitionPreviousState', this.resolveMethodDelayMs);
  }

  abortRma(): Promise<{error: RmadErrorCode}> {
    return this.methods.resolveMethodWithDelay(
        'abortRma', this.resolveMethodDelayMs);
  }

  /**
   * Sets the value that will be returned when calling abortRma().
   */
  setAbortRmaResult(error: RmadErrorCode): void {
    this.methods.setResult('abortRma', {error: error});
  }

  beginFinalization(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'beginFinalization', State.kWelcomeScreen);
  }

  trackConfiguredNetworks(): void {
    this.trackConfiguredNetworksCalled = true;
  }

  getTrackConfiguredNetworks(): boolean {
    return this.trackConfiguredNetworksCalled;
  }

  networkSelectionComplete(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'networkSelectionComplete', State.kConfigureNetwork);
  }

  getCurrentOsVersion(): Promise<{version: string}> {
    return this.methods.resolveMethod('getCurrentOsVersion');
  }

  setGetCurrentOsVersionResult(version: string|null) {
    this.methods.setResult('getCurrentOsVersion', {version: version});
  }

  checkForOsUpdates(): Promise<{updateAvailable: boolean, version: string}> {
    return this.methods.resolveMethod('checkForOsUpdates');
  }

  setCheckForOsUpdatesResult(version: string) {
    this.methods.setResult(
        'checkForOsUpdates', {updateAvailable: true, version});
  }

  updateOs(): Promise<{updateStarted: boolean}> {
    if (this.automaticallyTriggerOsUpdateObservation) {
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kCheckingForUpdate, 0.1, UpdateErrorCode.kSuccess,
          500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kUpdateAvailable, 0.3, UpdateErrorCode.kSuccess,
          1000);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kDownloading, 0.5, UpdateErrorCode.kSuccess, 1500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kVerifying, 0.7, UpdateErrorCode.kSuccess, 2000);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kFinalizing, 0.9, UpdateErrorCode.kSuccess, 2500);
      this.triggerOsUpdateObserver(
          OsUpdateOperation.kUpdatedNeedReboot, 1.0, UpdateErrorCode.kSuccess,
          3000);
    }
    return this.methods.resolveMethod('updateOs');
  }

  setUpdateOsResult(started: boolean): void {
    this.methods.setResult('updateOs', {updateStarted: started});
  }

  updateOsSkipped(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('updateOsSkipped', State.kUpdateOs);
  }

  setSameOwner(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('setSameOwner', State.kChooseDestination);
  }

  setDifferentOwner(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'setDifferentOwner', State.kChooseDestination);
  }

  setWipeDevice(_shouldWipeDevice: boolean):
      Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('setWipeDevice', State.kChooseWipeDevice);
  }

  manualDisableWriteProtectAvailable(): Promise<{available: boolean}> {
    return this.methods.resolveMethod('manualDisableWriteProtectAvailable');
  }

  setManualDisableWriteProtectAvailableResult(available: boolean) {
    this.methods.setResult(
        'manualDisableWriteProtectAvailable', {available: available});
  }

  setManuallyDisableWriteProtect(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'setManuallyDisableWriteProtect',
        State.kChooseWriteProtectDisableMethod);
  }

  setRsuDisableWriteProtect(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'setRsuDisableWriteProtect', State.kChooseWriteProtectDisableMethod);
  }

  getRsuDisableWriteProtectChallenge(): Promise<{challenge: string}> {
    return this.methods.resolveMethod('getRsuDisableWriteProtectChallenge');
  }

  setGetRsuDisableWriteProtectChallengeResult(challenge: string) {
    this.methods.setResult(
        'getRsuDisableWriteProtectChallenge', {challenge: challenge});
  }

  getRsuDisableWriteProtectHwid(): Promise<{hwid: string}> {
    return this.methods.resolveMethod('getRsuDisableWriteProtectHwid');
  }

  setGetRsuDisableWriteProtectHwidResult(hwid: string) {
    this.methods.setResult('getRsuDisableWriteProtectHwid', {hwid: hwid});
  }

  getRsuDisableWriteProtectChallengeQrCode(): Promise<{qrCodeData: number[]}> {
    return this.methods.resolveMethod(
        'getRsuDisableWriteProtectChallengeQrCode');
  }

  setGetRsuDisableWriteProtectChallengeQrCodeResponse(qrCodeData: number[]) {
    this.methods.setResult(
        'getRsuDisableWriteProtectChallengeQrCode', {qrCodeData: qrCodeData});
  }

  setRsuDisableWriteProtectCode(_code: string):
      Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'setRsuDisableWriteProtectCode', State.kEnterRSUWPDisableCode);
  }

  writeProtectManuallyDisabled(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'writeProtectManuallyDisabled', State.kWaitForManualWPDisable);
  }

  getWriteProtectDisableCompleteAction():
      Promise<{action: WriteProtectDisableCompleteAction}> {
    return this.methods.resolveMethod('getWriteProtectDisableCompleteAction');
  }

  setGetWriteProtectDisableCompleteAction(
      action: WriteProtectDisableCompleteAction): void {
    this.methods.setResult(
        'getWriteProtectDisableCompleteAction', {action: action});
  }

  confirmManualWpDisableComplete(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'confirmManualWpDisableComplete', State.kWPDisableComplete);
  }

  getComponentList(): Promise<{components: Component[]}> {
    this.methods.setResult('getComponentList', {components: this.components});
    return this.methods.resolveMethod('getComponentList');
  }

  setGetComponentListResult(components: Component[]): void {
    this.components = components;
  }

  setComponentList(_components: Component[]):
      Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'setComponentList', State.kSelectComponents);
  }

  reworkMainboard(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'reworkMainboard', State.kSelectComponents);
  }

  roFirmwareUpdateComplete(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'roFirmwareUpdateComplete', State.kUpdateRoFirmware);
  }

  shutdownForRestock(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('shutdownForRestock', State.kRestock);
  }

  continueFinalizationAfterRestock(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'continueFinalizationAfterRestock', State.kRestock);
  }

  getRegionList(): Promise<{regions: string[]}> {
    return this.methods.resolveMethod('getRegionList');
  }

  setGetRegionListResult(regions: string[]): void {
    this.methods.setResult('getRegionList', {regions: regions});
  }

  getSkuList(): Promise<{skus: bigint[]}> {
    return this.methods.resolveMethod('getSkuList');
  }

  setGetSkuListResult(skus: bigint[]): void {
    this.methods.setResult('getSkuList', {skus: skus});
  }

  getCustomLabelList(): Promise<{customLabels: string[]}> {
    return this.methods.resolveMethod('getCustomLabelList');
  }

  setGetCustomLabelListResult(customLabels: string[]): void {
    this.methods.setResult('getCustomLabelList', {customLabels: customLabels});
  }

  getSkuDescriptionList(): Promise<{skuDescriptions: string[]}> {
    return this.methods.resolveMethod('getSkuDescriptionList');
  }

  setGetSkuDescriptionListResult(skuDescriptions: string[]): void {
    this.methods.setResult(
        'getSkuDescriptionList', {skuDescriptions: skuDescriptions});
  }

  getOriginalSerialNumber(): Promise<{serialNumber: string}> {
    return this.methods.resolveMethod('getOriginalSerialNumber');
  }

  setGetOriginalSerialNumberResult(serialNumber: string): void {
    this.methods.setResult(
        'getOriginalSerialNumber', {serialNumber: serialNumber});
  }

  getOriginalRegion(): Promise<{regionIndex: number}> {
    return this.methods.resolveMethod('getOriginalRegion');
  }

  setGetOriginalRegionResult(regionIndex: number): void {
    this.methods.setResult('getOriginalRegion', {regionIndex: regionIndex});
  }

  getOriginalSku(): Promise<{skuIndex: number}> {
    return this.methods.resolveMethod('getOriginalSku');
  }

  setGetOriginalSkuResult(skuIndex: number): void {
    this.methods.setResult('getOriginalSku', {skuIndex: skuIndex});
  }

  getOriginalCustomLabel(): Promise<{customLabelIndex: number}> {
    return this.methods.resolveMethod('getOriginalCustomLabel');
  }

  setGetOriginalCustomLabelResult(customLabelIndex: number): void {
    this.methods.setResult(
        'getOriginalCustomLabel', {customLabelIndex: customLabelIndex});
  }

  getOriginalDramPartNumber(): Promise<{dramPartNumber: string}> {
    return this.methods.resolveMethod('getOriginalDramPartNumber');
  }

  setGetOriginalDramPartNumberResult(dramPartNumber: string): void {
    this.methods.setResult(
        'getOriginalDramPartNumber', {dramPartNumber: dramPartNumber});
  }

  getOriginalFeatureLevel(): Promise<{originalFeatureLevel: FeatureLevel}> {
    return this.methods.resolveMethod('getOriginalFeatureLevel');
  }

  setGetOriginalFeatureLevelResult(featureLevel: FeatureLevel): void {
    this.methods.setResult(
        'getOriginalFeatureLevel', {originalFeatureLevel: featureLevel});
  }

  setDeviceInformation(
    _serialNumber: string,
    _regionIndex: number,
    _skuIndex: number,
    _customLabelIndex: number,
    _dramPartNumber: string,
    _isChassisBranded: boolean,
    _hwComplianceVersion: number,
  ): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'setDeviceInformation', State.kUpdateDeviceInformation);
  }

  getCalibrationComponentList():
      Promise<{components: CalibrationComponentStatus[]}> {
    return this.methods.resolveMethod('getCalibrationComponentList');
  }

  setGetCalibrationComponentListResult(components:
                                           CalibrationComponentStatus[]): void {
    this.methods.setResult(
        'getCalibrationComponentList', {components: components});
  }

  getCalibrationSetupInstructions():
      Promise<{instructions: CalibrationSetupInstruction}> {
    return this.methods.resolveMethod('getCalibrationSetupInstructions');
  }

  setGetCalibrationSetupInstructionsResult(instructions:
                                               CalibrationSetupInstruction): void {
    this.methods.setResult(
        'getCalibrationSetupInstructions', {instructions: instructions});
  }

  /**
   * The fake does not use the status list parameter, the fake data is never
   * updated.
   */
  startCalibration(_components: CalibrationComponentStatus[]):
      Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'startCalibration', State.kCheckCalibration);
  }

  runCalibrationStep(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'runCalibrationStep', State.kSetupCalibration);
  }

  continueCalibration(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'continueCalibration', State.kRunCalibration);
  }

  calibrationComplete(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'calibrationComplete', State.kRunCalibration);
  }

  retryProvisioning(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'retryProvisioning', State.kProvisionDevice);
  }

  provisioningComplete(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'provisioningComplete', State.kProvisionDevice);
  }

  finalizationComplete(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('finalizationComplete', State.kFinalize);
  }

  retryFinalization(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('retryFinalization', State.kFinalize);
  }

  writeProtectManuallyEnabled(): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod(
        'writeProtectManuallyEnabled', State.kWaitForManualWPEnable);
  }

  getLog(): Promise<{log: string, error: RmadErrorCode}> {
    return this.methods.resolveMethod('getLog');
  }

  setGetLogResult(log: string): void {
    this.methods.setResult('getLog', {log: log, error: RmadErrorCode.kOk});
  }

  saveLog(): Promise<{savePath: FilePath, error: RmadErrorCode}> {
    return this.methods.resolveMethod('saveLog');
  }

  setSaveLogResult(savePath: FilePath): void {
    this.methods.setResult(
        'saveLog', {savePath: savePath, error: RmadErrorCode.kOk});
  }

  getPowerwashRequired():
      Promise<{powerwashRequired: boolean, error: RmadErrorCode}> {
    return this.methods.resolveMethod('getPowerwashRequired');
  }

  setGetPowerwashRequiredResult(powerwashRequired: boolean): void {
    this.methods.setResult(
        'getPowerwashRequired',
        {powerwashRequired: powerwashRequired, error: RmadErrorCode.kOk});
  }

  launchDiagnostics(): void {
    console.log('(Fake) Launching diagnostics...');
  }

  /**
   * The fake does not use the status list parameter, the fake data is never
   * updated.
   */
  endRma(_shutdownMethod: ShutdownMethod): Promise<{stateResult: StateResult}> {
    return this.getNextStateForMethod('endRma', State.kRepairComplete);
  }

  criticalErrorExitToLogin(): Promise<{error: RmadErrorCode}> {
    return this.methods.resolveMethodWithDelay(
        'criticalErrorExitToLogin', this.resolveMethodDelayMs);
  }

  criticalErrorReboot(): Promise<{error: RmadErrorCode}> {
    return this.methods.resolveMethodWithDelay(
        'criticalErrorReboot', this.resolveMethodDelayMs);
  }

  shutDownAfterHardwareError(): void {
    console.log('(Fake) Shutting down...');
  }

  get3pDiagnosticsProvider(): Promise<{provider: string | null}> {
    return this.methods.resolveMethodWithDelay(
        'get3pDiagnosticsProvider', this.resolveMethodDelayMs);
  }

  setGet3pDiagnosticsProviderResult(provider: string|null): void {
    this.methods.setResult('get3pDiagnosticsProvider', {provider});
  }

  getInstallable3pDiagnosticsAppPath(): Promise<{appPath: FilePath | null}> {
    return this.methods.resolveMethod('getInstallable3pDiagnosticsAppPath');
  }

  setInstallable3pDiagnosticsAppPath(appPath: FilePath|null): void {
    this.methods.setResult('getInstallable3pDiagnosticsAppPath', {appPath});
  }

  installLastFound3pDiagnosticsApp():
      Promise<{appInfo: Shimless3pDiagnosticsAppInfo | null}> {
    return this.methods.resolveMethod('installLastFound3pDiagnosticsApp');
  }

  setInstallLastFound3pDiagnosticsApp(appInfo: Shimless3pDiagnosticsAppInfo|
                                      null): void {
    this.methods.setResult('installLastFound3pDiagnosticsApp', {appInfo});
  }

  completeLast3pDiagnosticsInstallation(isApproved: boolean): Promise<void> {
    this.lastCompleteLast3pDiagnosticsInstallationApproval = isApproved;
    return Promise.resolve();
  }

  getLastCompleteLast3pDiagnosticsInstallationApproval(): boolean {
    assert(this.lastCompleteLast3pDiagnosticsInstallationApproval !== null);
    return this.lastCompleteLast3pDiagnosticsInstallationApproval as boolean;
  }

  show3pDiagnosticsApp(): Promise<{result: Show3pDiagnosticsAppResult}> {
    this.wasShow3pDiagnosticsAppCalled = true;
    return this.methods.resolveMethod('show3pDiagnosticsApp');
  }

  getWasShow3pDiagnosticsAppCalled(): boolean {
    return this.wasShow3pDiagnosticsAppCalled;
  }

  setShow3pDiagnosticsAppResult(result: Show3pDiagnosticsAppResult): void {
    this.methods.setResult('show3pDiagnosticsApp', {result});
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveError.
   */
  observeError(remote: ErrorObserverRemote): void {
    this.observables.observe(
        'ErrorObserver_onError', (error: RmadErrorCode) => {
          remote.onError(error);
        });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveOsUpdate.
   */
  observeOsUpdateProgress(remote: OsUpdateObserverRemote): void {
    this.observables.observe(
        'OsUpdateObserver_onOsUpdateProgressUpdated',
        (operation: OsUpdateOperation, progress: number,
         errorCode: UpdateErrorCode) => {
          remote.onOsUpdateProgressUpdated(operation, progress, errorCode);
        });
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveRoFirmwareUpdateProgress.
   */
  observeRoFirmwareUpdateProgress(remote: UpdateRoFirmwareObserverRemote): void {
    this.observables.observe(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged',
        (status: UpdateRoFirmwareStatus) => {
          remote.onUpdateRoFirmwareStatusChanged(status);
        });
    if (this.automaticallyTriggerUpdateRoFirmwareObservation) {
      this.triggerUpdateRoFirmwareObserver(UpdateRoFirmwareStatus.kWaitUsb, 0);
      this.triggerUpdateRoFirmwareObserver(
          UpdateRoFirmwareStatus.kUpdating, 1000);
      this.triggerUpdateRoFirmwareObserver(
          UpdateRoFirmwareStatus.kRebooting, 2000);
      this.triggerUpdateRoFirmwareObserver(
          UpdateRoFirmwareStatus.kComplete, 3000);
    }
  }

  /**
   * Trigger update ro firmware observations when an observer is added.
   */
  enableAautomaticallyTriggerUpdateRoFirmwareObservation(): void {
    this.automaticallyTriggerUpdateRoFirmwareObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveCalibration.
   */
  observeCalibrationProgress(remote: CalibrationObserverRemote): void {
    this.observables.observe(
        'CalibrationObserver_onCalibrationUpdated',
        (componentStatus: CalibrationComponentStatus) => {
          remote.onCalibrationUpdated(componentStatus);
        });
    this.observables.observe(
        'CalibrationObserver_onCalibrationStepComplete',
        (status: CalibrationOverallStatus) => {
          remote.onCalibrationStepComplete(status);
        });
    if (this.automaticallyTriggerCalibrationObservation) {
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationWaiting,
            progress: 0.0,
          },
          1000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.2,
          },
          2000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.4,
          },
          3000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.6,
          },
          4000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationInProgress,
            progress: 0.8,
          },
          5000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kLidAccelerometer,
            status: CalibrationStatus.kCalibrationWaiting,
            progress: 0.0,
          },
          6000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationComplete,
            progress: 0.5,
          },
          7000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseAccelerometer,
            status: CalibrationStatus.kCalibrationFailed,
            progress: 1.0,
          },
          8000);
      this.triggerCalibrationObserver(
          {
            component: ComponentType.kBaseGyroscope,
            status: CalibrationStatus.kCalibrationSkip,
            progress: 1.0,
          },
          9000);
      this.triggerCalibrationOverallObserver(
          CalibrationOverallStatus.kCalibrationOverallComplete, 10000);
    }
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveProvisioning.
   */
  observeProvisioningProgress(remote: ProvisioningObserverRemote): void {
    this.observables.observe(
        'ProvisioningObserver_onProvisioningUpdated',
        (status: ProvisioningStatus, progress: number,
         error: ProvisioningError) => {
          remote.onProvisioningUpdated(status, progress, error);
        });
    if (this.automaticallyTriggerProvisioningObservation) {
      // Fake progress over 4 seconds.
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.25, ProvisioningError.kUnknown,
          1000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.5, ProvisioningError.kUnknown,
          2000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kInProgress, 0.75, ProvisioningError.kUnknown,
          3000);
      this.triggerProvisioningObserver(
          ProvisioningStatus.kComplete, 1.0, ProvisioningError.kUnknown, 4000);
    }
  }

  /**
   * Trigger provisioning observations when an observer is added.
   */
  enableAutomaticallyTriggerProvisioningObservation(): void {
    this.automaticallyTriggerProvisioningObservation = true;
  }

  /**
   * Trigger calibration observations when an observer is added.
   */
  enableAutomaticallyTriggerCalibrationObservation(): void {
    this.automaticallyTriggerCalibrationObservation = true;
  }

  /**
   * Trigger OS update observations when an OS update is started.
   */
  enableAutomaticallyTriggerOsUpdateObservation(): void {
    this.automaticallyTriggerOsUpdateObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveHardwareWriteProtectionState.
   */
  observeHardwareWriteProtectionState(
      remote: HardwareWriteProtectionStateObserverRemote): void {
    this.observables.observe(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged',
        (enabled: boolean) => {
          remote.onHardwareWriteProtectionStateChanged(enabled);
        });
    if (this.states &&
        this.automaticallyTriggerDisableWriteProtectionObservation) {
      assert(this.stateIndex < this.states.length);
      this.triggerHardwareWriteProtectionObserver(
          this.states[this.stateIndex].state === State.kWaitForManualWPEnable,
          3000);
    }
  }

  /**
   * Trigger a disable write protection observation when an observer is added.
   */
  enableAutomaticallyTriggerDisableWriteProtectionObservation(): void {
    this.automaticallyTriggerDisableWriteProtectionObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObservePowerCableState.
   */
  observePowerCableState(remote: PowerCableStateObserverRemote): void {
    this.observables.observe(
        'PowerCableStateObserver_onPowerCableStateChanged',
        (pluggedIn: boolean) => {
          remote.onPowerCableStateChanged(pluggedIn);
        });
    if (this.automaticallyTriggerPowerCableStateObservation) {
      this.triggerPowerCableObserver(false, 1000);
      this.triggerPowerCableObserver(true, 10000);
      this.triggerPowerCableObserver(false, 15000);
    }
  }

  /**
   * Trigger a disable power cable state observations when an observer is added.
   */
  enableAutomaticallyTriggerPowerCableStateObservation(): void {
    this.automaticallyTriggerPowerCableStateObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveExternalDiskState.
   */
  observeExternalDiskState(remote: ExternalDiskStateObserverRemote): void {
    this.observables.observe(
        'ExternalDiskStateObserver_onExternalDiskStateChanged',
        (detected: boolean) => {
          remote.onExternalDiskStateChanged(detected);
        });

    this.triggerExternalDiskObserver(true, 10000);
    this.triggerExternalDiskObserver(false, 15000);
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveHardwareVerificationStatus.
   */
  observeHardwareVerificationStatus(
      remote: HardwareVerificationStatusObserverRemote): void {
    this.observables.observe(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult',
        (isCompliant: boolean, errorMessage: string) => {
          remote.onHardwareVerificationResult(isCompliant, errorMessage);
        });
    if (this.automaticallyTriggerHardwareVerificationStatusObservation) {
      this.triggerHardwareVerificationStatusObserver(true, '', 3000);
    }
  }


  /**
   * Trigger a hardware verification observation when an observer is added.
   */
  enableAutomaticallyTriggerHardwareVerificationStatusObservation(): void {
    this.automaticallyTriggerHardwareVerificationStatusObservation = true;
  }

  /**
   * Implements ShimlessRmaServiceInterface.ObserveFinalizationStatus.
   */
  observeFinalizationStatus(remote: FinalizationObserverRemote): void {
    this.observables.observe(
        'FinalizationObserver_onFinalizationUpdated',
        (status: FinalizationStatus, progress: number,
         error: FinalizationError) => {
          remote.onFinalizationUpdated(status, progress, error);
        });
    if (this.automaticallyTriggerFinalizationObservation) {
      this.triggerFinalizationObserver(
          FinalizationStatus.kInProgress, 0.25, FinalizationError.kUnknown,
          1000);
      this.triggerFinalizationObserver(
          FinalizationStatus.kInProgress, 0.75, FinalizationError.kUnknown,
          2000);
      this.triggerFinalizationObserver(
          FinalizationStatus.kComplete, 1.0, FinalizationError.kUnknown, 3000);
    }
  }

  /**
   * Trigger a finalization progress observation when an observer is added.
   */
  enableAutomaticallyTriggerFinalizationObservation(): void {
    this.automaticallyTriggerFinalizationObservation = true;
  }

  /**
   * Causes the error observer to fire after a delay.
   */
  triggerErrorObserver(error: RmadErrorCode, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs('ErrorObserver_onError', error, delayMs);
  }

  /**
   * Causes the OS update observer to fire after a delay.
   */
  triggerOsUpdateObserver(
      operation: OsUpdateOperation, progress: number, error: UpdateErrorCode,
      delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'OsUpdateObserver_onOsUpdateProgressUpdated',
        [operation, progress, error], delayMs);
  }

  /**
   * Causes the update RO firmware observer to fire after a delay.
   */
  triggerUpdateRoFirmwareObserver(
      status: UpdateRoFirmwareStatus, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged', status,
        delayMs);
  }

  /**
   * Causes the calibration observer to fire after a delay.
   */
  triggerCalibrationObserver(
      componentStatus: CalibrationComponentStatus, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'CalibrationObserver_onCalibrationUpdated', componentStatus, delayMs);
  }

  /**
   * Causes the calibration overall observer to fire after a delay.
   */
  triggerCalibrationOverallObserver(
      status: CalibrationOverallStatus, delayMs: number) {
    return this.triggerObserverAfterMs(
        'CalibrationObserver_onCalibrationStepComplete', status, delayMs);
  }

  /**
   * Causes the provisioning observer to fire after a delay.
   */
  triggerProvisioningObserver(
      status: ProvisioningStatus, progress: number, error: ProvisioningError,
      delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'ProvisioningObserver_onProvisioningUpdated', [status, progress, error],
        delayMs);
  }

  /**
   * Causes the hardware write protection observer to fire after a delay.
   */
  triggerHardwareWriteProtectionObserver(enabled: boolean, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged',
        enabled, delayMs);
  }

  /**
   * Causes the power cable observer to fire after a delay.
   */
  triggerPowerCableObserver(pluggedIn: boolean, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'PowerCableStateObserver_onPowerCableStateChanged', pluggedIn, delayMs);
  }

  /**
   * Causes the external disk observer to fire after a delay.
   */
  triggerExternalDiskObserver(detected: boolean, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'ExternalDiskStateObserver_onExternalDiskStateChanged', detected,
        delayMs);
  }

  /**
   * Causes the hardware verification observer to fire after a delay.
   */
  triggerHardwareVerificationStatusObserver(
      isCompliant: boolean, errorMessage: string, delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult',
        [isCompliant, errorMessage], delayMs);
  }

  /**
   * Causes the finalization observer to fire after a delay.
   */
  triggerFinalizationObserver(
      status: FinalizationStatus, progress: number, error: FinalizationError,
      delayMs: number): Promise<unknown> {
    return this.triggerObserverAfterMs(
        'FinalizationObserver_onFinalizationUpdated', [status, progress, error],
        delayMs);
  }

  /**
   * Causes an observer to fire after a delay.
   */
  triggerObserverAfterMs<T>(method: string, result: T, delayMs: number): Promise<unknown> {
    const setDataTriggerAndResolve = function(
        service: FakeShimlessRmaService, resolve: any) {
      service.observables.setObservableData(method, [result]);
      service.observables.trigger(method);
      resolve();
    };

    return new Promise((resolve) => {
      if (delayMs === 0) {
        setDataTriggerAndResolve(this, resolve);
      } else {
        setTimeout(() => {
          setDataTriggerAndResolve(this, resolve);
        }, delayMs);
      }
    });
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset(): void {
    this.registerMethods();
    this.registerObservables();

    this.states = [];
    this.stateIndex = 0;

    // This state data is more complicated so the behavior of the get/set
    // methods is a little different than other fakes in that they don't return
    // undefined by default.
    this.components = [];
    this.setGetLogResult('');
    this.setSaveLogResult({'path': ''});

    this.lastCompleteLast3pDiagnosticsInstallationApproval = null;
    this.wasShow3pDiagnosticsAppCalled = false;
    this.setGet3pDiagnosticsProviderResult(null);
    this.setAsyncOperationDelayMs(/* delayMs= */ 0);
  }

  /**
   * Setup method resolvers.
   */
  private registerMethods(): void {
    this.methods = new FakeMethodResolver();

    this.methods.register('getCurrentState');
    this.methods.register('transitionPreviousState');

    this.methods.register('abortRma');

    this.methods.register('canExit');
    this.methods.register('canGoBack');

    this.methods.register('beginFinalization');

    this.methods.register('trackConfiguredNetworks');
    this.methods.register('networkSelectionComplete');

    this.methods.register('getCurrentOsVersion');
    this.methods.register('checkForOsUpdates');
    this.methods.register('updateOs');
    this.methods.register('updateOsSkipped');

    this.methods.register('setSameOwner');
    this.methods.register('setDifferentOwner');
    this.methods.register('setWipeDevice');

    this.methods.register('setManuallyDisableWriteProtect');
    this.methods.register('setRsuDisableWriteProtect');
    this.methods.register('getRsuDisableWriteProtectChallenge');
    this.methods.register('getRsuDisableWriteProtectHwid');
    this.methods.register('getRsuDisableWriteProtectChallengeQrCode');
    this.methods.register('setRsuDisableWriteProtectCode');

    this.methods.register('writeProtectManuallyDisabled');

    this.methods.register('getWriteProtectDisableCompleteAction');
    this.methods.register('confirmManualWpDisableComplete');

    this.methods.register('shutdownForRestock');
    this.methods.register('continueFinalizationAfterRestock');

    this.methods.register('getComponentList');
    this.methods.register('setComponentList');
    this.methods.register('reworkMainboard');

    this.methods.register('roFirmwareUpdateComplete');

    this.methods.register('getRegionList');
    this.methods.register('getSkuList');
    this.methods.register('getCustomLabelList');
    this.methods.register('getSkuDescriptionList');
    this.methods.register('getOriginalSerialNumber');
    this.methods.register('getOriginalRegion');
    this.methods.register('getOriginalSku');
    this.methods.register('getOriginalCustomLabel');
    this.methods.register('getOriginalDramPartNumber');
    this.methods.register('getOriginalFeatureLevel');
    this.methods.register('setDeviceInformation');

    this.methods.register('getCalibrationComponentList');
    this.methods.register('getCalibrationSetupInstructions');
    this.methods.register('startCalibration');
    this.methods.register('runCalibrationStep');
    this.methods.register('continueCalibration');
    this.methods.register('calibrationComplete');

    this.methods.register('retryProvisioning');
    this.methods.register('provisioningComplete');

    this.methods.register('retryFinalization');
    this.methods.register('finalizationComplete');

    this.methods.register('writeProtectManuallyEnabled');

    this.methods.register('getLog');
    this.methods.register('saveLog');
    this.methods.register('getPowerwashRequired');
    this.methods.register('endRma');

    // Critical error handling
    this.methods.register('criticalErrorExitToLogin');
    this.methods.register('criticalErrorReboot');

    this.methods.register('shutDownAfterHardwareError');

    this.methods.register('get3pDiagnosticsProvider');
    this.methods.register('getInstallable3pDiagnosticsAppPath');
    this.methods.register('installLastFound3pDiagnosticsApp');
    this.methods.register('show3pDiagnosticsApp');
  }

  /**
   * Setup observables.
   */
  private registerObservables(): void {
    if (this.observables) {
      this.observables.stopAllTriggerIntervals();
    }
    this.observables = new FakeObservables();
    this.observables.register('ErrorObserver_onError');
    this.observables.register('OsUpdateObserver_onOsUpdateProgressUpdated');
    this.observables.register(
        'UpdateRoFirmwareObserver_onUpdateRoFirmwareStatusChanged');
    this.observables.register('CalibrationObserver_onCalibrationUpdated');
    this.observables.register('CalibrationObserver_onCalibrationStepComplete');
    this.observables.register('ProvisioningObserver_onProvisioningUpdated');
    this.observables.register(
        'HardwareWriteProtectionStateObserver_onHardwareWriteProtectionStateChanged');
    this.observables.register(
        'PowerCableStateObserver_onPowerCableStateChanged');
    this.observables.register(
        'ExternalDiskStateObserver_onExternalDiskStateChanged');
    this.observables.register(
        'HardwareVerificationStatusObserver_onHardwareVerificationResult');
    this.observables.register('FinalizationObserver_onFinalizationUpdated');
  }

  private getNextStateForMethod(method: string, expectedState: State):
      Promise<{stateResult: StateResult}> {
    if (this.states.length === 0) {
      this.setFakeStateForMethod(
          method, State.kUnknown, false, false, RmadErrorCode.kRmaNotRequired);
    } else if (this.stateIndex >= this.states.length - 1) {
      // It should not be possible for stateIndex to be out of range unless
      // there is a bug in the fake.
      assert(this.stateIndex < this.states.length);
      const state = this.states[this.stateIndex];
      this.setFakeStateForMethod(
          method, state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kTransitionFailed);
    } else if (this.states[this.stateIndex].state !== expectedState) {
      // Error: Called in wrong state.
      const state = this.states[this.stateIndex];
      this.setFakeStateForMethod(
          method, state.state, state.canExit, state.canGoBack,
          RmadErrorCode.kRequestInvalid);
    } else {
      // Success.
      this.stateIndex++;
      const state = this.states[this.stateIndex];
      this.setFakeStateForMethod(
          method, state.state, state.canExit, state.canGoBack, state.error);
    }
    return this.methods.resolveMethodWithDelay(
        method, this.resolveMethodDelayMs);
  }

  /**
   * Sets the value that will be returned when calling getCurrent().
   */
  private setFakeCurrentState(
      state: State, canExit: boolean, canGoBack: boolean,
      error: RmadErrorCode): void {
    this.setFakeStateForMethod(
        'getCurrentState', state, canExit, canGoBack, error);
  }

  /**
   * Sets the value that will be returned when calling
   * transitionPreviousState().
   */
  private setFakePrevState(
      state: State, canExit: boolean, canGoBack: boolean,
      error: RmadErrorCode): void {
    this.setFakeStateForMethod(
        'transitionPreviousState', state, canExit, canGoBack, error);
  }

  /**
   * Sets the value that will be returned when calling state specific functions
   * that update state. e.g. setSameOwner()
   */
  private setFakeStateForMethod(
      method: string, state: State, canExit: boolean, canGoBack: boolean,
      error: RmadErrorCode): void {
    this.methods.setResult(method, {
      stateResult: {
        state: state,
        canExit: canExit,
        canGoBack: canGoBack,
        error: error,
      },
    });
  }
}
