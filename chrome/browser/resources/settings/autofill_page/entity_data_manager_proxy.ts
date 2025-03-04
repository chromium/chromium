// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AttributeType = chrome.autofillPrivate.AttributeType;
type EntityInstance = chrome.autofillPrivate.EntityInstance;
type EntityInstanceWithLabels = chrome.autofillPrivate.EntityInstanceWithLabels;
type EntityType = chrome.autofillPrivate.EntityType;

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
   * Returns a list of all possible entities that exist.
   */
  getAllEntityTypes(): Promise<EntityType[]>;

  /**
   * Returns a list of all possible attributes that can be set on an entity.
   */
  getAllAttributeTypesForEntity(entityType: number): Promise<AttributeType[]>;
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

  getAllEntityTypes() {
    return chrome.autofillPrivate.getAllEntityTypes();
  }

  getAllAttributeTypesForEntity(entityType: number) {
    return chrome.autofillPrivate.getAllAttributeTypesForEntity(entityType);
  }

  static getInstance() {
    return instance || (instance = new EntityDataManagerProxyImpl());
  }

  static setInstance(obj: EntityDataManagerProxy) {
    instance = obj;
  }
}

let instance: EntityDataManagerProxy|null = null;
