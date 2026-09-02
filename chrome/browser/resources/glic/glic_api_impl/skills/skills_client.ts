// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {enumFromClient, enumToClient} from '../../enum_conversions.js';
import {SkillsClientReceiver, SkillsHandlerRemote, SkillSource as SkillSourceMojo} from '../../glic.mojom-webui.js';
import type {Skill as SkillMojo, SkillPreview as SkillPreviewMojo, SkillsClientInterface, WebClientHandlerRemote} from '../../glic.mojom-webui.js';
import type {CreateSkillRequest, GlicBrowserHost, GlicBrowserSkills, ObservableValue, Skill, SkillPreview, SkillsWebClientEvent, UpdateSkillRequest} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import {optionalToClient, timeToClient, urlToClient} from '../host/conversions.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';
import type {WebClientInitialStatePrivate} from '../request_types.js';

export function skillPreviewToClient(
    preview: SkillPreviewMojo, isContextual: boolean): SkillPreview {
  return {
    ...preview,
    source: enumToClient(preview.source),
    curatedBy: optionalToClient(preview.curatedBy),
    imageUrl: urlToClient(preview.imageUrl),
    category: optionalToClient(preview.category),
    creationTime: timeToClient(preview.creationTime),
    isContextual,
  };
}

export function skillToClient(skill: SkillMojo): Skill {
  return {
    preview: skillPreviewToClient(skill.preview, false),
    prompt: skill.prompt,
    sourceSkillId: optionalToClient(skill.sourceSkillId),
  };
}

class GlicBrowserSkillsImpl implements GlicBrowserSkills {
  constructor(private host: GlicBrowserHostSkills) {}

  async createSkill(request: CreateSkillRequest): Promise<void> {
    const result = await this.host.getSkillsHandler().createSkill({
      id: request.id ?? '',
      name: request.name ?? '',
      icon: request.icon ?? '',
      prompt: request.prompt,
      description: request.description ?? '',
      source: enumFromClient(request.source) ?? SkillSourceMojo.kUnknown,
    });
    if (!result.modalOpened) {
      throw new Error('createSkill: failed to open dialog');
    }
  }

  async updateSkill(request: UpdateSkillRequest): Promise<void> {
    const result = await this.host.getSkillsHandler().updateSkill(request);
    if (!result.modalOpened) {
      throw new Error('updateSkill: failed to open dialog');
    }
  }

  showManageSkillsUi(): void {
    this.host.getSkillsHandler().showManageSkillsUi();
  }

  showBrowseSkillsUi(): void {
    this.host.getSkillsHandler().showBrowseSkillsUi();
  }

  async getSkill(id: string): Promise<Skill> {
    const {skill: mojoSkill} = await this.host.getSkillsHandler().getSkill(id);
    if (!mojoSkill) {
      throw new Error('getSkill: failed');
    }
    return skillToClient(mojoSkill);
  }

  recordSkillsWebClientEvent(event: SkillsWebClientEvent): void {
    this.host.getSkillsHandler().recordSkillsWebClientEvent(
        enumFromClient(event));
  }

  getSkillPreviews(): ObservableValue<SkillPreview[]> {
    return this.host.skillPreviews;
  }
}

export class GlicBrowserHostSkills implements SkillsClientInterface,
                                              Partial<GlicBrowserHost> {
  private skillsHandler?: SkillsHandlerRemote;
  private skillsClientReceiver?: SkillsClientReceiver;
  private cachedSkillPreviews: SkillPreview[] = [];
  private cachedContextualSkillPreviews: SkillPreview[] = [];

  skillPreviews = ObservableValueImpl.withNoValue<SkillPreview[]>();
  skillToInvoke = ObservableValueImpl.withNoValue<Skill>();
  private skillsObservable =
      ObservableValueImpl.withValue<GlicBrowserSkills|undefined>(undefined);
  private skillsInstance = new GlicBrowserSkillsImpl(this);

  initialize(
      initialState: WebClientInitialStatePrivate,
      handler: WebClientHandlerRemote) {
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

    this.skillsHandler = maybeWrapWithLogging(
        new SkillsHandlerRemote(), {prefix: 'SkillsHandler'});
    this.skillsClientReceiver = new SkillsClientReceiver(this);
    handler.createSkillsHandler(
        this.skillsHandler.$.bindNewPipeAndPassReceiver(),
        this.skillsClientReceiver.$.bindNewPipeAndPassRemote());
  }

  destroySkills(): void {
    if (this.skillsHandler) {
      this.skillsHandler.$.close();
      this.skillsHandler = undefined;
    }
    if (this.skillsClientReceiver) {
      this.skillsClientReceiver.$.close();
      this.skillsClientReceiver = undefined;
    }
  }

  getSkillsHandler(): SkillsHandlerRemote {
    assert(this.skillsHandler);
    return this.skillsHandler;
  }

  notifySkillPreviewsChanged(skillPreviews: SkillPreviewMojo[]): void {
    this.cachedSkillPreviews =
        skillPreviews.map((p) => skillPreviewToClient(p, false));
    this.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifyContextualSkillPreviewsChanged(skillPreviews: SkillPreviewMojo[]):
      void {
    this.cachedContextualSkillPreviews =
        skillPreviews.map((p) => skillPreviewToClient(p, true));
    this.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillPreviewChanged(skillPreview: SkillPreviewMojo): void {
    const clientPreview = skillPreviewToClient(skillPreview, false);
    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === clientPreview.id);

    if (index !== -1) {
      // SkillPreview with the same ID exists, replace it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        clientPreview,
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    } else {
      // SkillPreview with this ID not found, add it to the cache.
      this.cachedSkillPreviews = [...this.cachedSkillPreviews, clientPreview];
    }

    // Signal the change to the host.
    this.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillDeleted(skillId: string): void {
    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillId);
    if (index !== -1) {
      // SkillPreview with the same ID exists, remove it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    }
    this.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillsEnabledChanged(enabled: boolean): void {
    this.setSkillsEnabled(enabled);
  }

  private combineSkillPreviews(): SkillPreview[] {
    return [...this.cachedContextualSkillPreviews, ...this.cachedSkillPreviews];
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
