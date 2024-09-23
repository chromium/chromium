// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert.js';

import {AcceleratorResultData, AcceleratorsUpdatedObserverRemote, EditDialogCompletedActions, PolicyUpdatedObserverRemote, Subactions, UserAction} from '../mojom-webui/shortcut_customization.mojom-webui.js';

import {Accelerator, AcceleratorCategory, AcceleratorConfigResult, AcceleratorSource, MetaKey, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';


/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

// Method names.
const ON_ACCELERATORS_UPDATED_METHOD_NAME =
    'AcceleratorsUpdatedObserver_OnAcceleratorsUpdated';
const ON_POLICY_UPDATED_METHOD_NAME =
    'PolicyUpdatedObserver_OnCustomizationPolicyUpdated';
export class FakeShortcutProvider implements ShortcutProviderInterface {
  private methods: FakeMethodResolver;
  private observables: FakeObservables = new FakeObservables();
  private acceleratorsUpdatedRemote: AcceleratorsUpdatedObserverRemote|null =
      null;
  private acceleratorsUpdatedPromise: Promise<void>|null = null;
  private policyUpdateRemote: PolicyUpdatedObserverRemote|null = null;
  private policyUpdatedPromise: Promise<void>|null = null;
  private restoreDefaultCallCount: number = 0;
  private preventProcessingAcceleratorsCallCount: number = 0;
  private addAcceleratorCallCount: number = 0;
  private removeAcceleratorCallCount: number = 0;
  private lastRecordedUserAction: UserAction;
  private lastRecordedMainCategory: AcceleratorCategory;
  private lastRecoredEditDialogActions: EditDialogCompletedActions;
  private lastRecordedIsAdd: boolean = false;
  private lastRecorededSubactions: Subactions;

  constructor() {
    this.methods = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods.register('getAccelerators');
    this.methods.register('getAcceleratorLayoutInfos');
    this.methods.register('isMutable');
    this.methods.register('isCustomizationAllowedByPolicy');
    this.methods.register('getMetaKeyToDisplay');
    this.methods.register('addAccelerator');
    this.methods.register('replaceAccelerator');
    this.methods.register('removeAccelerator');
    this.methods.register('restoreDefault');
    this.methods.register('restoreAllDefaults');
    this.methods.register('addObserver');
    this.methods.register('addPolicyObserver');
    this.methods.register('preventProcessingAccelerators');
    this.methods.register('getConflictAccelerator');
    this.methods.register('getDefaultAcceleratorsForId');
    this.methods.register('recordUserAction');
    this.methods.register('recordMainCategoryNavigation');
    this.methods.register('recordEditDialogCompetedActions');
    this.methods.register('recordAddOrEditSubactions');
    this.registerObservables();
  }

  registerObservables(): void {
    this.observables.register(ON_ACCELERATORS_UPDATED_METHOD_NAME);
    this.observables.register(ON_POLICY_UPDATED_METHOD_NAME);
  }

  // Disable all observers and reset provider to initial state.
  reset(): void {
    this.restoreDefaultCallCount = 0;
    this.preventProcessingAcceleratorsCallCount = 0;
    this.addAcceleratorCallCount = 0;
    this.removeAcceleratorCallCount = 0;
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  triggerOnAcceleratorUpdated(): void {
    this.observables.trigger(ON_ACCELERATORS_UPDATED_METHOD_NAME);
  }

  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}> {
    return this.methods.resolveMethod('getAcceleratorLayoutInfos');
  }

  getAccelerators(): Promise<{config: MojoAcceleratorConfig}> {
    return this.methods.resolveMethod('getAccelerators');
  }

  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}> {
    this.methods.setResult(
        'isMutable', {isMutable: source !== AcceleratorSource.kBrowser});
    return this.methods.resolveMethod('isMutable');
  }

  isCustomizationAllowedByPolicy():
      Promise<{isCustomizationAllowedByPolicy: boolean}> {
    return this.methods.resolveMethod('isCustomizationAllowedByPolicy');
  }

  getMetaKeyToDisplay(): Promise<{metaKey: MetaKey}> {
    return this.methods.resolveMethod('getMetaKeyToDisplay');
  }

  addObserver(observer: AcceleratorsUpdatedObserverRemote): void {
    this.acceleratorsUpdatedPromise = this.observe(
        ON_ACCELERATORS_UPDATED_METHOD_NAME,
        (config: MojoAcceleratorConfig) => {
          observer.onAcceleratorsUpdated(config);
        });
  }

  addPolicyObserver(observer: PolicyUpdatedObserverRemote): void {
    this.policyUpdatedPromise =
        this.observe(ON_POLICY_UPDATED_METHOD_NAME, () => {
          observer.onCustomizationPolicyUpdated();
        });
  }

  getAcceleratorsUpdatedPromiseForTesting(): Promise<void> {
    assert(this.acceleratorsUpdatedPromise);
    return this.acceleratorsUpdatedPromise;
  }

  getPolicyUpdatedPromiseForTesting(): Promise<void> {
    assert(this.policyUpdatedPromise);
    return this.policyUpdatedPromise;
  }

  // Set the value that will be retuned when `onAcceleratorsUpdated()` is
  // called.
  setFakeAcceleratorsUpdated(config: MojoAcceleratorConfig[]): void {
    this.observables.setObservableData(
        ON_ACCELERATORS_UPDATED_METHOD_NAME, config);
  }

  setFakePolicyUpdated(): void {
    this.observables.setObservableData(ON_POLICY_UPDATED_METHOD_NAME, [true]);
  }

  addAccelerator(
      _source: AcceleratorSource, _actionId: number,
      _accelerator: Accelerator): Promise<{result: AcceleratorResultData}> {
    ++this.addAcceleratorCallCount;
    return this.methods.resolveMethod('addAccelerator');
  }

  replaceAccelerator(
      _source: AcceleratorSource, _actionId: number,
      _old_accelerator: Accelerator,
      _new_accelerator: Accelerator): Promise<{result: AcceleratorResultData}> {
    // Always return kSuccess in this fake.
    return this.methods.resolveMethod('replaceAccelerator');
  }

  removeAccelerator(): Promise<{result: AcceleratorResultData}> {
    // Always return kSuccess in this fake.
    ++this.removeAcceleratorCallCount;
    return this.methods.resolveMethod('removeAccelerator');
  }

  restoreDefault(_source: AcceleratorSource, _actionId: number):
      Promise<{result: AcceleratorResultData}> {
    ++this.restoreDefaultCallCount;
    return this.methods.resolveMethod('restoreDefault');
  }

  restoreAllDefaults(): Promise<{result: AcceleratorResultData}> {
    // Always return kSuccess in this fake.
    const result: AcceleratorResultData = {
      result: AcceleratorConfigResult.kSuccess,
      shortcutName: null,
    };
    this.methods.setResult('restoreAllDefaults', {result});
    return this.methods.resolveMethod('restoreAllDefaults');
  }

  recordUserAction(userAction: UserAction): void {
    this.lastRecordedUserAction = userAction;
  }

  recordEditDialogCompletedActions(completed_actions:
                                       EditDialogCompletedActions): void {
    this.lastRecoredEditDialogActions = completed_actions;
  }

  getLastEditDialogCompletedActions(): EditDialogCompletedActions {
    return this.lastRecoredEditDialogActions;
  }

  getLatestRecordedAction(): UserAction {
    return this.lastRecordedUserAction;
  }

  recordMainCategoryNavigation(category: AcceleratorCategory): void {
    this.lastRecordedMainCategory = category;
  }

  getLatestMainCategoryNavigated(): AcceleratorCategory {
    return this.lastRecordedMainCategory;
  }

  recordAddOrEditSubactions(isAdd: boolean, subactions: Subactions): void {
    this.lastRecordedIsAdd = isAdd;
    this.lastRecorededSubactions = subactions;
  }

  getLastRecordedIsAdd(): boolean {
    return this.lastRecordedIsAdd;
  }

  getLastRecordedSubactions(): Subactions {
    return this.lastRecorededSubactions;
  }

  preventProcessingAccelerators(_preventProcessingAccelerators: boolean):
      Promise<void> {
    ++this.preventProcessingAcceleratorsCallCount;
    return this.methods.resolveMethod('preventProcessingAccelerators');
  }

  getConflictAccelerator(
      _source: AcceleratorSource, _actionId: number,
      _accelerator: Accelerator): Promise<{result: AcceleratorResultData}> {
    return this.methods.resolveMethod('getConflictAccelerator');
  }

  getDefaultAcceleratorsForId(
      _actionId: number,
      ): Promise<{accelerators: Accelerator[]}> {
    return this.methods.resolveMethod('getDefaultAcceleratorsForId');
  }

  /**
   * Set the config result that will be returned when calling
   * `getConflictAccelerator()`.
   */
  setFakeGetConflictAccelerator(result: AcceleratorResultData): void {
    this.methods.setResult('getConflictAccelerator', {result});
  }

  /**
   * Set the default accelerators that will be returned when calling
   * `getDefaultAcceleratorsForId()`.
   */
  setFakeGetDefaultAcceleratorsForId(accelerators: Accelerator[]): void {
    this.methods.setResult('getDefaultAcceleratorsForId', {accelerators});
  }

  /**
   * Sets the value that will be returned when calling
   * getAccelerators().
   */
  setFakeAcceleratorConfig(config: MojoAcceleratorConfig): void {
    this.methods.setResult('getAccelerators', {config});
  }

  /**
   * Sets the value that will be returned when calling
   * getAcceleratorLayoutInfos().
   */
  setFakeAcceleratorLayoutInfos(layoutInfos: MojoLayoutInfo[]): void {
    this.methods.setResult('getAcceleratorLayoutInfos', {layoutInfos});
  }

  getRestoreDefaultCallCount(): number {
    return this.restoreDefaultCallCount;
  }

  getPreventProcessingAcceleratorsCallCount(): number {
    return this.preventProcessingAcceleratorsCallCount;
  }

  getAddAcceleratorCallCount(): number {
    return this.addAcceleratorCallCount;
  }

  getRemoveAcceleratorCallCount(): number {
    return this.removeAcceleratorCallCount;
  }

  setFakeMetaKeyToDisplay(metaKey: MetaKey): void {
    this.methods.setResult('getMetaKeyToDisplay', {metaKey});
  }

  setFakeAddAcceleratorResult(result: AcceleratorResultData): void {
    this.methods.setResult('addAccelerator', {result});
  }

  setFakeReplaceAcceleratorResult(result: AcceleratorResultData): void {
    this.methods.setResult('replaceAccelerator', {result});
  }

  setFakeRestoreDefaultResult(result: AcceleratorResultData): void {
    this.methods.setResult('restoreDefault', {result});
  }

  setFakeRemoveAcceleratorResult(result: AcceleratorResultData): void {
    this.methods.setResult('removeAccelerator', {result});
  }

  setFakeIsCustomizationAllowedByPolicy(isCustomizationAllowedByPolicy:
                                            boolean): void {
    this.methods.setResult(
        'isCustomizationAllowedByPolicy', {isCustomizationAllowedByPolicy});
  }

  // Sets up an observer for methodName.
  private observe(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables.observe(methodName, callback);
      resolve();
    });
  }
}
