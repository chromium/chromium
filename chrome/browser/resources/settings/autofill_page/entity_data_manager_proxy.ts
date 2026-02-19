// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AttributeType = chrome.autofillPrivate.AttributeType;
type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

export type EntityInstancesChangedListener =
    (entityInstances: EntityInstanceWithLabels[]) => void;

/**
 * This interface defines the autofill API wrapper that combines entity data
 * manager related methods.
 */
export interface EntityDataManagerProxy {
  /**
   * Adds a new entity instance if it doesn't exist yet. Otherwise, it updates
   * the entity instance.
   */
  addOrUpdateEntityInstance(entityInstance: EntityInstance): Promise<void>;

  /**
   * Remove the entity instance by its id.
   */
  removeEntityInstance(guid: string): void;

  /**
   * Returns the user's entity instances with labels.
   */
  loadEntityInstances(): Promise<EntityInstanceWithLabels[]>;

  /**
   * Returns the entity instance by its guid.
   */
  getEntityInstanceByGuid(guid: string): Promise<EntityInstance>;

  /**
   * Returns a list of all enabled entity types which are not read only.
   */
  getWritableEntityTypes(): Promise<EntityType[]>;

  /**
   * Returns a list of all attribute types that can be set on an entity
   * instance.
   */
  getAllAttributeTypesForEntityTypeName(entityTypeName: number):
      Promise<AttributeType[]>;

  /**
   * Returns a list of all attribute types that are required to save an entity
   * instance. The list represents a disjunction: presenting any one attribute
   * is sufficient.
   */
  getRequiredAttributeTypesForEntityTypeName(entityTypeName: number):
      Promise<AttributeType[]>;

  /**
   * Adds a listener to changes in the entity instances.
   */
  addEntityInstancesChangedListener(listener: EntityInstancesChangedListener):
      void;

  /**
   * Removes a listener to changes in the entity instances.
   */
  removeEntityInstancesChangedListener(
      listener: EntityInstancesChangedListener): void;

  /**
   * Authenticates the user before viewing entity data. Returns true if
   * authentication was successful or if no authentication was required.
   */
  authenticateUserBeforeViewingEntityData(): Promise<boolean>;

  /**
   * Updates the pref that controls whether users need to authenticate
   * to view sensitive entity information. This method itself triggers
   * reauthentication
   */
  toggleAutofillAiReauthRequirement(): void;

  /**
   * Gets the opt-in status for AutofillAi for the current user.
   */
  getOptInStatus(): Promise<boolean>;

  /**
   * Sets the opt-in status for AutofillAi for the current user. Returns whether
   * setting was successful, which more precisely means whether the user is
   * eligible to opt into Autofill AI.
   */
  setOptInStatus(optedIn: boolean): Promise<boolean>;

  /**
   * Gets the opt-in status for walletable pass detection for the current user.
   */
  getWalletablePassDetectionOptInStatus(): Promise<boolean>;

  /**
   * Sets the opt-in status for walletable pass detection for the current user.
   */
  setWalletablePassDetectionOptInStatus(optedIn: boolean): Promise<boolean>;
}

export class EntityDataManagerProxyImpl implements EntityDataManagerProxy {
  addOrUpdateEntityInstance(entityInstance: EntityInstance): Promise<void> {
    return chrome.autofillPrivate.addOrUpdateEntityInstance(entityInstance);
  }

  removeEntityInstance(guid: string) {
    return chrome.autofillPrivate.removeEntityInstance(guid);
  }

  loadEntityInstances() {
    return chrome.autofillPrivate.loadEntityInstances();
  }

  getEntityInstanceByGuid(guid: string) {
    return chrome.autofillPrivate.getEntityInstanceByGuid(guid);
  }

  getWritableEntityTypes() {
    return chrome.autofillPrivate.getWritableEntityTypes();
  }

  getAllAttributeTypesForEntityTypeName(entityTypeName: number) {
    return chrome.autofillPrivate.getAllAttributeTypesForEntityTypeName(
        entityTypeName);
  }

  getRequiredAttributeTypesForEntityTypeName(entityTypeName: number):
      Promise<AttributeType[]> {
    return chrome.autofillPrivate.getRequiredAttributeTypesForEntityTypeName(
        entityTypeName);
  }

  addEntityInstancesChangedListener(listener: EntityInstancesChangedListener) {
    chrome.autofillPrivate.onEntityInstancesChanged.addListener(listener);
  }

  removeEntityInstancesChangedListener(
      listener: EntityInstancesChangedListener) {
    chrome.autofillPrivate.onEntityInstancesChanged.removeListener(listener);
  }

  authenticateUserBeforeViewingEntityData() {
    return chrome.autofillPrivate.authenticateUserBeforeViewingEntityData();
  }

  toggleAutofillAiReauthRequirement() {
    return chrome.autofillPrivate.toggleAutofillAiReauthRequirement();
  }

  getOptInStatus() {
    return chrome.autofillPrivate.getAutofillAiOptInStatus();
  }

  setOptInStatus(optedIn: boolean) {
    return chrome.autofillPrivate.setAutofillAiOptInStatus(optedIn);
  }

  getWalletablePassDetectionOptInStatus() {
    return chrome.autofillPrivate.getWalletablePassDetectionOptInStatus();
  }

  setWalletablePassDetectionOptInStatus(optedIn: boolean) {
    return chrome.autofillPrivate.setWalletablePassDetectionOptInStatus(
        optedIn);
  }

  static getInstance() {
    return instance || (instance = new EntityDataManagerProxyImpl());
  }

  static setInstance(obj: EntityDataManagerProxy) {
    instance = obj;
  }
}

let instance: EntityDataManagerProxy|null = null;
