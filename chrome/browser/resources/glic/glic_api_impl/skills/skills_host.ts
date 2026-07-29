// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {enumFromClient, enumToClient} from '../../enum_conversions.js';
import {SkillSource as SkillSourceMojo} from '../../glic.mojom-webui.js';
import type {Skill as SkillMojo, SkillPreview as SkillPreviewMojo, SkillsClientInterface, SkillsHandlerRemote} from '../../glic.mojom-webui.js';
import type {CreateSkillRequest, Skill, SkillPreview, SkillsWebClientEvent, UpdateSkillRequest} from '../../glic_api/glic_api.js';
import {optionalToClient, urlToClient} from '../host/conversions.js';
import type {MessageHandlerInterface} from '../transport/messaging.js';
import type {PostMessageRemote} from '../transport/post_message_transport.js';

import type {SkillsClient, SkillsHost} from './skills_types.js';

export function skillPreviewToClient(
    preview: SkillPreviewMojo, isContextual: boolean): SkillPreview {
  return {
    ...preview,
    source: enumToClient(preview.source),
    curatedBy: optionalToClient(preview.curatedBy),
    imageUrl: urlToClient(preview.imageUrl),
    category: optionalToClient(preview.category),
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

export class SkillsHostMessageHandler implements
    MessageHandlerInterface<SkillsHost> {
  constructor(private skillsHandler: SkillsHandlerRemote) {}

  async createSkill(request: {
    request: CreateSkillRequest,
  }): Promise<{modalOpened: boolean}> {
    const mojoRequest = {
      id: request.request.id ?? '',
      name: request.request.name ?? '',
      icon: request.request.icon ?? '',
      prompt: request.request.prompt,
      description: request.request.description ?? '',
      source:
          enumFromClient(request.request.source) ?? SkillSourceMojo.kUnknown,
    };
    return await this.skillsHandler.createSkill(mojoRequest);
  }

  async updateSkill(request: {
    request: UpdateSkillRequest,
  }): Promise<{modalOpened: boolean}> {
    return await this.skillsHandler.updateSkill(request.request);
  }

  showManageSkillsUi(_request: void): void {
    this.skillsHandler.showManageSkillsUi();
  }

  showBrowseSkillsUi(_request: void): void {
    this.skillsHandler.showBrowseSkillsUi();
  }

  async getSkill(request: {
    id: string,
  }): Promise<{skill?: Skill}> {
    const {skill: mojoSkill} = await this.skillsHandler.getSkill(request.id);
    if (!mojoSkill) {
      return {};
    }
    return {
      skill: skillToClient(mojoSkill),
    };
  }

  recordSkillsWebClientEvent(request: {
    event: SkillsWebClientEvent,
  }): void {
    this.skillsHandler.recordSkillsWebClientEvent(
        enumFromClient(request.event));
  }
}

export class SkillsClientImpl implements SkillsClientInterface {
  constructor(private sender: PostMessageRemote<SkillsClient>) {}

  notifySkillPreviewsChanged(skillPreviews: SkillPreviewMojo[]): void {
    const clientPreviews =
        skillPreviews.map((p) => skillPreviewToClient(p, false));
    this.sender.requestNoResponse(
        'notifySkillPreviewsChanged', {skillPreviews: clientPreviews});
  }

  notifyContextualSkillPreviewsChanged(skillPreviews: SkillPreviewMojo[]):
      void {
    const clientPreviews =
        skillPreviews.map((p) => skillPreviewToClient(p, true));
    this.sender.requestNoResponse(
        'notifyContextualSkillPreviewsChanged',
        {contextualSkillPreviews: clientPreviews});
  }

  notifySkillPreviewChanged(skillPreview: SkillPreviewMojo): void {
    this.sender.requestNoResponse('notifySkillPreviewChanged', {
      skillPreview: skillPreviewToClient(skillPreview, false),
    });
  }

  notifySkillDeleted(skillId: string): void {
    this.sender.requestNoResponse('notifySkillDeleted', {skillId});
  }

  notifySkillToInvokeChanged(skill: SkillMojo): void {
    this.sender.requestNoResponse('notifySkillToInvokeChanged', {
      skill: skillToClient(skill),
    });
  }
}
