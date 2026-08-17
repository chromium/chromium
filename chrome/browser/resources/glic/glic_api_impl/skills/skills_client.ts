// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {CreateSkillRequest, GlicBrowserHost, GlicBrowserSkills, ObservableValue, Skill, SkillPreview, SkillsWebClientEvent, UpdateSkillRequest} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import type {WebClientInitialStatePrivate} from '../request_types.js';
import type {MessageHandlerInterface} from '../transport/messaging.js';
import type {PendingReceiver, PendingRemote, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import {SkillsClientDef} from './skills_types.js';
import type {SkillsClient, SkillsHost} from './skills_types.js';

export class SkillsWebClientMessageHandler implements
    MessageHandlerInterface<SkillsClient> {
  private cachedSkillPreviews: SkillPreview[] = [];
  private cachedContextualSkillPreviews: SkillPreview[] = [];

  constructor(private host: GlicBrowserHostSkills) {}

  notifySkillPreviewsChanged(payload: {
    skillPreviews: SkillPreview[],
  }): void {
    this.cachedSkillPreviews = payload.skillPreviews;
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifyContextualSkillPreviewsChanged(payload: {
    contextualSkillPreviews: SkillPreview[],
  }): void {
    this.cachedContextualSkillPreviews = payload.contextualSkillPreviews;
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillPreviewChanged(payload: {
    skillPreview: SkillPreview,
  }): void {
    const skillPreview = payload.skillPreview;

    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillPreview.id);

    if (index !== -1) {
      // SkillPreview with the same ID exists, replace it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        skillPreview,
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    } else {
      // SkillPreview with this ID not found, add it to the cache.
      this.cachedSkillPreviews = [...this.cachedSkillPreviews, skillPreview];
    }

    // Signal the change to the host.
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillDeleted(payload: {
    skillId: string,
  }): void {
    const skillId = payload.skillId;
    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillId);
    if (index !== -1) {
      // SkillPreview with the same ID exists, remove it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    }
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillsEnabledChanged(payload: {
    enabled: boolean,
  }): void {
    this.host.setSkillsEnabled(payload.enabled);
  }

  private combineSkillPreviews() {
    return [...this.cachedContextualSkillPreviews, ...this.cachedSkillPreviews];
  }
}

class GlicBrowserSkillsImpl implements GlicBrowserSkills {
  constructor(private host: GlicBrowserHostSkills) {}

  async createSkill(request: CreateSkillRequest): Promise<void> {
    const result = await this.host.getRemote().requestWithResponse(
        'createSkill', {request});
    if (!result.modalOpened) {
      throw new Error('createSkill: failed to open dialog');
    }
  }

  async updateSkill(request: UpdateSkillRequest): Promise<void> {
    const result = await this.host.getRemote().requestWithResponse(
        'updateSkill', {request});
    if (!result.modalOpened) {
      throw new Error('updateSkill: failed to open dialog');
    }
  }

  showManageSkillsUi(): void {
    this.host.getRemote().requestNoResponse('showManageSkillsUi', undefined);
  }

  showBrowseSkillsUi(): void {
    this.host.getRemote().requestNoResponse('showBrowseSkillsUi', undefined);
  }

  async getSkill(id: string): Promise<Skill> {
    const result =
        await this.host.getRemote().requestWithResponse('getSkill', {id});
    if (!result.skill) {
      throw new Error('getSkill: failed');
    }
    return result.skill;
  }

  recordSkillsWebClientEvent(event: SkillsWebClientEvent): void {
    this.host.getRemote().requestNoResponse(
        'recordSkillsWebClientEvent', {event});
  }

  getSkillPreviews(): ObservableValue<SkillPreview[]> {
    return this.host.skillPreviews;
  }
}

export class GlicBrowserHostSkills implements Partial<GlicBrowserHost> {
  private skillsRemote?: PostMessageRemote<SkillsHost>;
  private skillsWebClientMessageHandler =
      new SkillsWebClientMessageHandler(this);
  skillPreviews = ObservableValueImpl.withNoValue<SkillPreview[]>();
  skillToInvoke = ObservableValueImpl.withNoValue<Skill>();
  private skillsObservable =
      ObservableValueImpl.withValue<GlicBrowserSkills|undefined>(undefined);
  private skillsInstance = new GlicBrowserSkillsImpl(this);

  initialize(
      initialState: WebClientInitialStatePrivate, router: PostMessageRouter,
      skillsRemote: PendingRemote<SkillsHost>|undefined,
      skillsReceiver: PendingReceiver<SkillsClient>|undefined) {
    this.setSkillsEnabled(initialState.enableSkills);

    // Support legacy behavior (which had these functions exist or not exist
    // based on feature enablement as of initialization).
    if (!initialState.enableSkills) {
      this.createSkill = undefined;
      this.updateSkill = undefined;
      this.showManageSkillsUi = undefined;
      this.showBrowseSkillsUi = undefined;
      this.getSkill = undefined;
      this.recordSkillsWebClientEvent = undefined;
      this.getSkillPreviews = undefined;
      this.getSkillToInvoke = undefined;
    }

    if (skillsRemote && skillsReceiver) {
      this.skillsRemote = router.newRemote(skillsRemote);
      router.newReceiver(
          skillsReceiver, this.skillsWebClientMessageHandler, SkillsClientDef);
    }
  }

  getRemote(): PostMessageRemote<SkillsHost> {
    assert(this.skillsRemote);
    return this.skillsRemote;
  }

  createSkill?(request: CreateSkillRequest): Promise<void> {
    return this.skillsInstance.createSkill(request);
  }

  updateSkill?(request: UpdateSkillRequest): Promise<void> {
    return this.skillsInstance.updateSkill(request);
  }

  showManageSkillsUi?(): void {
    this.skillsInstance.showManageSkillsUi();
  }

  showBrowseSkillsUi?(): void {
    this.skillsInstance.showBrowseSkillsUi();
  }

  getSkill?(id: string): Promise<Skill> {
    return this.skillsInstance.getSkill(id);
  }

  recordSkillsWebClientEvent?(event: SkillsWebClientEvent): void {
    this.skillsInstance.recordSkillsWebClientEvent(event);
  }

  getSkillPreviews?(): ObservableValue<SkillPreview[]> {
    return this.skillsInstance.getSkillPreviews();
  }

  getSkillToInvoke?(): ObservableValue<Skill> {
    return this.skillToInvoke;
  }

  skills(): ObservableValue<GlicBrowserSkills|undefined> {
    return this.skillsObservable;
  }

  setSkillsEnabled(enabled: boolean) {
    this.skillsObservable.assignAndSignal(
        enabled ? this.skillsInstance : undefined);
  }
}
