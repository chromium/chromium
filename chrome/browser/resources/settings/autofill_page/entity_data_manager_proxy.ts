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
  addOrUpdateEntityInstance(entityInstance: EntityInstance): void;

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
   * Gets the opt-in status for AutofillAi for the current user.
   */
  getOptInStatus(): Promise<boolean>;

  /**
   * Sets the opt-in status for AutofillAi for the current user. Returns whether
   * setting was successful, which more precisely means whether the user is
   * eligible to opt into Autofill AI.
   */
  setOptInStatus(optedIn: boolean): Promise<boolean>;
}

export class EntityDataManagerProxyImpl implements EntityDataManagerProxy {
  addOrUpdateEntityInstance(entityInstance: EntityInstance) {
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

  addEntityInstancesChangedListener(listener: EntityInstancesChangedListener) {
    chrome.autofillPrivate.onEntityInstancesChanged.addListener(listener);
  }

  removeEntityInstancesChangedListener(
      listener: EntityInstancesChangedListener) {
    chrome.autofillPrivate.onEntityInstancesChanged.removeListener(listener);
  }

  getOptInStatus() {
    return chrome.autofillPrivate.getAutofillAiOptInStatus();
  }

  setOptInStatus(optedIn: boolean) {
    return chrome.autofillPrivate.setAutofillAiOptInStatus(optedIn);
  }

  static getInstance() {
    return instance || (instance = new EntityDataManagerProxyImpl());
  }

  static setInstance(obj: EntityDataManagerProxy) {
    instance = obj;
  }
}

let instance: EntityDataManagerProxy|null = null;
